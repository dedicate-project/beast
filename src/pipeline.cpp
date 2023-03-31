#include <beast/pipeline.hpp>

// Standard
#include <algorithm>
#include <stdexcept>

namespace beast {

void Pipeline::addPipe(const std::shared_ptr<Pipe>& pipe) {
  if (std::find(pipes_.begin(), pipes_.end(), pipe) != pipes_.end()) {
    throw std::invalid_argument("Pipe already in this pipeline.");
  }

  pipes_.push_back(pipe);
}

void Pipeline::connectPipes(
    const std::shared_ptr<Pipe>& source_pipe, uint32_t source_slot_index,
    const std::shared_ptr<Pipe>& destination_pipe, uint32_t destination_slot_index) {
  // Ensure that the pipes are both present already.
  if (std::find(pipes_.begin(), pipes_.end(), source_pipe) == pipes_.end()) {
    throw std::invalid_argument("Source Pipe not in this Pipeline.");
  }

  if (std::find(pipes_.begin(), pipes_.end(), destination_pipe) == pipes_.end()) {
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

  Connection connection{source_pipe, source_slot_index, destination_pipe, destination_slot_index};
  connections_.push_back(connection);
}

const std::list<std::shared_ptr<Pipe>>& Pipeline::getPipes() const {
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

}  // namespace beast
