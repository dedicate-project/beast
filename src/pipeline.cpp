#include <beast/pipeline.hpp>

// Standard
#include <algorithm>
#include <stdexcept>
#include <thread>

namespace beast {

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
  managed_pipe->should_run = false;
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

  std::shared_ptr<Connection> connection = std::make_shared<Connection>();
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
  if (is_running_) {
    throw std::invalid_argument("Pipeline is already running, cannot start it.");
  }

  for (std::shared_ptr<ManagedPipe>& managed_pipe : pipes_) {
    if (!managed_pipe->is_running) {
      managed_pipe->should_run = true;
      std::thread thread(&Pipeline::pipelineWorker, this, std::ref(managed_pipe));
      std::swap(managed_pipe->thread, thread);
      managed_pipe->is_running = true;
    }
  }

  is_running_ = true;
}

void Pipeline::stop() {
  if (!is_running_) {
    throw std::invalid_argument("Pipeline is not running, cannot stop it.");
  }

  for (const std::shared_ptr<ManagedPipe>& managed_pipe : pipes_) {
    if (managed_pipe->is_running) {
      managed_pipe->should_run = false;
      managed_pipe->thread.join();
      managed_pipe->is_running = false;
    }
  }

  is_running_ = false;
}

bool Pipeline::isRunning() const { return is_running_; }

bool Pipeline::pipeIsInPipeline(const std::shared_ptr<Pipe>& pipe) const {
  return std::find_if(pipes_.begin(),
                      pipes_.end(),
                      [&pipe](const std::shared_ptr<ManagedPipe>& managed_pipe) {
                        return managed_pipe->pipe == pipe;
                      }) != pipes_.end();
}

void Pipeline::findConnections(const std::shared_ptr<ManagedPipe>& managed_pipe,
                               std::vector<std::shared_ptr<Connection>>& source_connections,
                               std::vector<std::shared_ptr<Connection>>& destination_connections) {
  for (const std::shared_ptr<Connection>& connection : connections_) {
    if (connection->destination_pipe == managed_pipe) {
      source_connections.push_back(connection);
    }
    if (connection->source_pipe == managed_pipe) {
      destination_connections.push_back(connection);
    }
  }
}

void Pipeline::processOutputSlots(
    const std::shared_ptr<ManagedPipe>& managed_pipe,
    const std::vector<std::shared_ptr<Connection>>& destination_connections) {
  for (uint32_t slot_index = 0; slot_index < managed_pipe->pipe->getOutputSlotCount();
       ++slot_index) {
    if (!managed_pipe->pipe->hasOutput(slot_index)) {
      continue;
    }
    auto destination_slot_connection_iter =
        std::find_if(destination_connections.begin(),
                     destination_connections.end(),
                     [slot_index](const std::shared_ptr<Connection>& connection) {
                       return connection->destination_slot_index == slot_index;
                     });

    if (destination_slot_connection_iter == destination_connections.end()) {
      continue;
    }

    std::shared_ptr<Connection>& destination_slot_connection = *destination_slot_connection_iter;
    std::scoped_lock lock(destination_slot_connection->buffer_mutex);
    while (
        managed_pipe->pipe->hasOutput(slot_index) &&
        (destination_slot_connection->buffer.size() < destination_slot_connection->buffer_size)) {
      auto data = managed_pipe->pipe->drawOutput(slot_index);
      destination_slot_connection->buffer.push_back(std::move(data));
    }
  }
}

void Pipeline::processInputSlots(
    const std::shared_ptr<ManagedPipe>& managed_pipe,
    const std::vector<std::shared_ptr<Connection>>& source_connections) {
  for (uint32_t slot_index = 0; slot_index < managed_pipe->pipe->getInputSlotCount();
       ++slot_index) {
    auto source_slot_connection_iter =
        std::find_if(source_connections.begin(),
                     source_connections.end(),
                     [slot_index](const std::shared_ptr<Connection>& connection) {
                       return connection->source_slot_index == slot_index;
                     });

    if (source_slot_connection_iter == source_connections.end()) {
      continue;
    }

    std::shared_ptr<Connection>& source_slot_connection = *source_slot_connection_iter;
    if (source_slot_connection->buffer.empty()) {
      continue;
    }

    std::scoped_lock lock(source_slot_connection->buffer_mutex);
    while (managed_pipe->pipe->inputHasSpace(slot_index) &&
           !source_slot_connection->buffer.empty()) {
      auto data = source_slot_connection->buffer.back();
      source_slot_connection->buffer.pop_back();
      managed_pipe->pipe->addInput(slot_index, data.data);
    }
  }
}

void Pipeline::pipelineWorker(const std::shared_ptr<ManagedPipe>& managed_pipe) {
  std::vector<std::shared_ptr<Connection>> source_connections;
  std::vector<std::shared_ptr<Connection>> destination_connections;
  findConnections(managed_pipe, source_connections, destination_connections);

  while (managed_pipe->should_run) {
    processOutputSlots(managed_pipe, destination_connections);
    processInputSlots(managed_pipe, source_connections);

    if (!managed_pipe->pipe->outputsAreSaturated() && managed_pipe->pipe->inputsAreSaturated()) {
      managed_pipe->pipe->execute();
    }

    // Limit cycle time.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

} // namespace beast
