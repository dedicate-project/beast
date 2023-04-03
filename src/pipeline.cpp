#include <beast/pipeline.hpp>

// Standard
#include <algorithm>
#include <stdexcept>

namespace beast {

void Pipeline::addPipe(const std::shared_ptr<Pipe>& pipe) {
  if (pipeIsInPipeline(pipe)) {
    throw std::invalid_argument("Pipe already in this pipeline.");
  }

  ManagedPipe managed_pipe;
  managed_pipe.pipe = pipe;
  managed_pipe.should_run = false;
  pipes_.push_back(std::move(managed_pipe));
}

void Pipeline::connectPipes(
    const std::shared_ptr<Pipe>& source_pipe, uint32_t source_slot_index,
    const std::shared_ptr<Pipe>& destination_pipe, uint32_t destination_slot_index,
    uint32_t buffer_size) {
  // Ensure that the pipes are both present already.
  if (!pipeIsInPipeline(source_pipe)) {
    throw std::invalid_argument("Source Pipe not in this Pipeline.");
  }

  if (!pipeIsInPipeline(destination_pipe)) {
    throw std::invalid_argument("Destination Pipe not in this Pipeline.");
  }

  // Ensure this connection doesn't exist yet.
  for (const Connection& connection : connections_) {
    if (connection.source_pipe == source_pipe &&
        connection.source_slot_index == source_slot_index) {
      throw std::invalid_argument("Source port already occupied on Pipe.");
    }

    if (connection.destination_pipe == destination_pipe &&
        connection.destination_slot_index == destination_slot_index) {
      throw std::invalid_argument("Destination port already occupied on Pipe.");
    }
  }

  Connection connection{
      source_pipe, source_slot_index, destination_pipe, destination_slot_index, {}};
  connection.buffer.reserve(buffer_size);
  connections_.push_back(connection);
}

const std::list<Pipeline::ManagedPipe>& Pipeline::getPipes() const {
  return pipes_;
}

const std::list<Pipeline::Connection>& Pipeline::getConnections() const {
  return connections_;
}

void Pipeline::start() {
  // TODO(fairlight1337): Implement actual pipeline start, with worker threads setup.
  if (is_running_) {
    throw std::invalid_argument("Pipeline is already running, cannot start it.");
  }
  
  is_running_ = true;
}

void Pipeline::stop() {
  // TODO(fairlight1337): Implement actual pipeline teardown, with worker threads setup.
  if (!is_running_) {
    throw std::invalid_argument("Pipeline is not running, cannot stop it.");
  }
  is_running_ = false;
}

bool Pipeline::isRunning() const {
  return is_running_;
}

bool Pipeline::pipeIsInPipeline(const std::shared_ptr<Pipe>& pipe) const {
  return std::find_if(pipes_.begin(), pipes_.end(),
                      [&pipe](const ManagedPipe& managed_pipe) {
                        return managed_pipe.pipe == pipe;
                      }) != pipes_.end();
}

void Pipeline::pipelineWorker(ManagedPipe& managed_pipe) {
  // Get all source and destination connections involving this pipe
  std::vector<Connection*> source_connections;
  std::vector<Connection*> destination_connections;
  for (Connection& connection : connections_) {
    if (connection.destination_pipe == managed_pipe.pipe) {
      source_connections.push_back(&connection);
    }
    if (connection.source_pipe == managed_pipe.pipe) {
      destination_connections.push_back(&connection);
    }
  }

  while (managed_pipe.should_run) {
    // Iterate through all output slots for this pipe.
    for (uint32_t slot_index = 0; slot_index < managed_pipe.pipe->getOutputSlotCount();
         ++slot_index) {
      // Skip this output slot if we dont' have any output.
      if (!managed_pipe.pipe->hasOutput(slot_index)) {
        continue;
      }
      // Get the destination connections for this slot.
      auto destination_slot_connection_iter =
          std::find_if(destination_connections.begin(), destination_connections.end(),
                       [slot_index](const Connection* connection) {
                         return connection->destination_slot_index == slot_index;
                       });
      if (destination_slot_connection_iter == destination_connections.end()) {
        // This slot doesn't seem to be connected. Ignore it.
        continue;
      }
      // Found a destination connection for this slot.
      Connection* destination_slot_connection = *destination_slot_connection_iter;
      // While we have output data, add it to the buffer if it has space.
      while (managed_pipe.pipe->hasOutput(slot_index) &&
             (destination_slot_connection->buffer.size() <
              destination_slot_connection->buffer.capacity())) {
        auto data = managed_pipe.pipe->drawOutput(slot_index);
        destination_slot_connection->buffer.push_back(std::move(data));
      }
    }
  }
}

}  // namespace beast
