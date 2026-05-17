#include <beast/pipeline.hpp>

// Standard
#include <algorithm>
#include <stdexcept>
#include <thread>

namespace beast {

Pipeline::Pipeline() { metrics_.measure_time_start = std::chrono::system_clock::now(); }

void Pipeline::addPipe(const std::string& name, const std::shared_ptr<Pipe>& pipe) {
  if (pipeIsInPipeline(pipe)) {
    throw std::invalid_argument("Pipe already in this pipeline.");
  }

  if (getManagedPipeByName(name)) {
    throw std::invalid_argument("Pipe name already exists in this pipeline");
  }

  auto managed_pipe = std::make_shared<ManagedPipe>();
  managed_pipe->name = name;
  managed_pipe->pipe = pipe;
  // should_run / is_running default-initialize to false via the in-class initializer on the
  // atomic members; no explicit reset needed here.
  pipes_.push_back(std::move(managed_pipe));
}

void Pipeline::connectPipes(const std::shared_ptr<Pipe>& source_pipe, uint32_t source_slot_index,
                            const std::shared_ptr<Pipe>& destination_pipe,
                            uint32_t destination_slot_index, uint32_t buffer_size) {
  // Ensure that the pipes are both present already.
  if (!pipeIsInPipeline(source_pipe)) {
    throw std::invalid_argument("Source Pipe not in this Pipeline.");
  }

  if (!pipeIsInPipeline(destination_pipe)) {
    throw std::invalid_argument("Destination Pipe not in this Pipeline.");
  }

  auto managed_source_pipe = getManagedPipeForPipe(source_pipe);
  auto managed_destination_pipe = getManagedPipeForPipe(destination_pipe);

  // Ensure this connection doesn't exist yet.
  for (const std::shared_ptr<Connection>& connection : connections_) {
    if (connection->source_pipe == managed_source_pipe &&
        connection->source_slot_index == source_slot_index) {
      throw std::invalid_argument("Source port already occupied on Pipe.");
    }

    if (connection->destination_pipe == managed_destination_pipe &&
        connection->destination_slot_index == destination_slot_index) {
      throw std::invalid_argument("Destination port already occupied on Pipe.");
    }
  }

  auto connection = std::make_shared<Connection>();
  connection->source_pipe = managed_source_pipe;
  connection->source_slot_index = source_slot_index;
  connection->destination_pipe = managed_destination_pipe;
  connection->destination_slot_index = destination_slot_index;
  connection->buffer_size = buffer_size;
  connections_.push_back(std::move(connection));
}

const std::list<std::shared_ptr<Pipeline::ManagedPipe>>& Pipeline::getPipes() const {
  return pipes_;
}

const std::list<std::shared_ptr<Pipeline::Connection>>& Pipeline::getConnections() const {
  return connections_;
}

void Pipeline::start() {
  if (is_running_.load(std::memory_order_acquire)) {
    throw std::invalid_argument("Pipeline is already running, cannot start it.");
  }

  for (std::shared_ptr<ManagedPipe>& managed_pipe : pipes_) {
    if (!managed_pipe->is_running.load(std::memory_order_acquire)) {
      managed_pipe->should_run.store(true, std::memory_order_release);
      // Capture the shared_ptr by VALUE in the thread - capturing by reference (the previous
      // `std::ref(managed_pipe)` form) keeps a dangling reference to a stack/list element whose
      // address could change between the thread launch and first execution, and prevents the
      // ManagedPipe from being kept alive purely by the worker.
      std::thread thread(&Pipeline::pipelineWorker, this, managed_pipe);
      std::swap(managed_pipe->thread, thread);
      managed_pipe->is_running.store(true, std::memory_order_release);
    }
  }

  is_running_.store(true, std::memory_order_release);
}

void Pipeline::stop() {
  if (!is_running_.load(std::memory_order_acquire)) {
    throw std::invalid_argument("Pipeline is not running, cannot stop it.");
  }

  for (const std::shared_ptr<ManagedPipe>& managed_pipe : pipes_) {
    if (managed_pipe->is_running.load(std::memory_order_acquire)) {
      managed_pipe->should_run.store(false, std::memory_order_release);
      managed_pipe->thread.join();
      managed_pipe->is_running.store(false, std::memory_order_release);
    }
  }

  is_running_.store(false, std::memory_order_release);
}

bool Pipeline::isRunning() const { return is_running_.load(std::memory_order_acquire); }

Pipeline::PipelineMetrics Pipeline::getMetrics() {
  std::scoped_lock lock(metrics_mutex_);
  // Cache and reset metrics object.
  PipelineMetrics metrics = metrics_;
  metrics_ = PipelineMetrics{};
  metrics_.measure_time_start = std::chrono::system_clock::now();

  return metrics;
}

bool Pipeline::pipeIsInPipeline(const std::shared_ptr<Pipe>& pipe) const {
  return std::find_if(pipes_.begin(),
                      pipes_.end(),
                      [&pipe](const std::shared_ptr<ManagedPipe>& managed_pipe) {
                        return managed_pipe->pipe == pipe;
                      }) != pipes_.end();
}

void Pipeline::findConnections(
    const std::shared_ptr<ManagedPipe>& managed_pipe,
    std::vector<std::shared_ptr<Connection>>& source_connections,
    std::vector<std::shared_ptr<Connection>>& destination_connections) const {
  for (const std::shared_ptr<Connection>& connection : connections_) {
    if (connection->destination_pipe == managed_pipe) {
      source_connections.push_back(connection);
    }
    if (connection->source_pipe == managed_pipe) {
      destination_connections.push_back(connection);
    }
  }
}

std::unordered_map<uint32_t, uint32_t> Pipeline::processOutputSlots(
    const std::shared_ptr<ManagedPipe>& managed_pipe,
    const std::vector<std::shared_ptr<Connection>>& destination_connections) {
  // `destination_connections` are connections in which `managed_pipe` is the SOURCE - i.e. the
  // connections that carry this pipe's output downstream. To find the connection corresponding to
  // a given local output slot, we therefore have to match the connection's `source_slot_index`
  // (the slot index on the SOURCE side) against our iteration variable. Matching against
  // `destination_slot_index` (which is an index on the downstream pipe) was a long-standing bug
  // that silently worked only when every pipe used slot 0 everywhere.
  std::unordered_map<uint32_t, uint32_t> metrics;
  for (uint32_t slot_index = 0; slot_index < managed_pipe->pipe->getOutputSlotCount();
       ++slot_index) {
    metrics[slot_index] = 0;
    if (!managed_pipe->pipe->hasOutput(slot_index)) {
      continue;
    }
    auto destination_slot_connection_iter =
        std::find_if(destination_connections.begin(),
                     destination_connections.end(),
                     [slot_index](const std::shared_ptr<Connection>& connection) {
                       return connection->source_slot_index == slot_index;
                     });

    if (destination_slot_connection_iter == destination_connections.end()) {
      continue;
    }

    const std::shared_ptr<Connection>& destination_slot_connection =
        *destination_slot_connection_iter;
    std::scoped_lock lock(destination_slot_connection->buffer_mutex);
    while (
        managed_pipe->pipe->hasOutput(slot_index) &&
        (destination_slot_connection->buffer.size() < destination_slot_connection->buffer_size)) {
      auto data = managed_pipe->pipe->drawOutput(slot_index);
      destination_slot_connection->buffer.push_back(std::move(data));
      metrics[slot_index]++;
    }
  }
  return metrics;
}

std::unordered_map<uint32_t, uint32_t>
Pipeline::processInputSlots(const std::shared_ptr<ManagedPipe>& managed_pipe,
                            const std::vector<std::shared_ptr<Connection>>& source_connections) {
  // `source_connections` are connections in which `managed_pipe` is the DESTINATION - i.e. the
  // connections that feed this pipe's input from upstream. To find the connection corresponding
  // to a given local input slot we therefore have to match the connection's
  // `destination_slot_index` (the slot index on the DESTINATION side) against our iteration
  // variable. Matching against `source_slot_index` was the symmetrical bug of the one in
  // `processOutputSlots`.
  std::unordered_map<uint32_t, uint32_t> metrics;
  for (uint32_t slot_index = 0; slot_index < managed_pipe->pipe->getInputSlotCount();
       ++slot_index) {
    metrics[slot_index] = 0;
    auto source_slot_connection_iter =
        std::find_if(source_connections.begin(),
                     source_connections.end(),
                     [slot_index](const std::shared_ptr<Connection>& connection) {
                       return connection->destination_slot_index == slot_index;
                     });

    if (source_slot_connection_iter == source_connections.end()) {
      continue;
    }

    const std::shared_ptr<Connection>& source_slot_connection = *source_slot_connection_iter;
    std::scoped_lock lock(source_slot_connection->buffer_mutex);
    // Drain in FIFO order: the buffer is filled via `push_back` in processOutputSlots, so the
    // oldest item lives at the front. The previous implementation popped from the back, which
    // made the pipeline behave as LIFO and starved the oldest candidates indefinitely.
    while (managed_pipe->pipe->inputHasSpace(slot_index) &&
           !source_slot_connection->buffer.empty()) {
      auto data = std::move(source_slot_connection->buffer.front());
      source_slot_connection->buffer.pop_front();
      managed_pipe->pipe->addInput(slot_index, data.data);
      metrics[slot_index]++;
    }
  }
  return metrics;
}

void Pipeline::pipelineWorker(const std::shared_ptr<ManagedPipe>& managed_pipe) {
  std::vector<std::shared_ptr<Connection>> source_connections;
  std::vector<std::shared_ptr<Connection>> destination_connections;
  findConnections(managed_pipe, source_connections, destination_connections);

  while (managed_pipe->should_run.load(std::memory_order_acquire)) {
    const auto input_metrics = processInputSlots(managed_pipe, source_connections);

    bool executed = false;
    if (!managed_pipe->pipe->outputsAreSaturated() && managed_pipe->pipe->inputsAreSaturated()) {
      try {
        managed_pipe->pipe->execute();
        executed = true;
      } catch (const std::exception&) {
        // Swallow exceptions from a single execution so a misbehaving pipe doesn't crash the
        // entire pipeline. We deliberately drop the message here - the pipeline has no logger,
        // and a future PR can add structured error reporting via the metrics channel.
        executed = false;
      }
    }

    const auto output_metrics = processOutputSlots(managed_pipe, destination_connections);

    // Limit cycle time. The fixed 10 ms wait is intentional: it gives upstream/downstream pipes a
    // chance to make progress without burning a core for purely structural work. A future PR can
    // replace this with a condition_variable backed off connection events.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Report metrics for this worker.
    reportMetrics(managed_pipe, executed, input_metrics, output_metrics);
  }
}

std::shared_ptr<Pipeline::ManagedPipe>
Pipeline::getManagedPipeForPipe(const std::shared_ptr<Pipe>& pipe) const {
  for (const auto& managed_pipe : pipes_) {
    if (managed_pipe->pipe == pipe) {
      return managed_pipe;
    }
  }
  return nullptr;
}

std::shared_ptr<Pipeline::ManagedPipe> Pipeline::getManagedPipeByName(std::string_view name) const {
  for (const auto& managed_pipe : pipes_) {
    if (managed_pipe->name == name) {
      return managed_pipe;
    }
  }
  return nullptr;
}

void Pipeline::reportMetrics(const std::shared_ptr<ManagedPipe>& managed_pipe, bool executed,
                             const std::unordered_map<uint32_t, uint32_t>& input_metrics,
                             const std::unordered_map<uint32_t, uint32_t>& output_metrics) {
  const std::string& pipe_name = managed_pipe->name;

  std::scoped_lock lock(metrics_mutex_);
  PipeMetrics& pipe_metrics = metrics_.pipes[pipe_name];

  if (executed) {
    pipe_metrics.execution_count++;
  }

  for (const auto& input_pair : input_metrics) {
    pipe_metrics.inputs_received[input_pair.first] += input_pair.second;
  }
  for (const auto& output_pair : output_metrics) {
    pipe_metrics.outputs_sent[output_pair.first] += output_pair.second;
  }
}

} // namespace beast
