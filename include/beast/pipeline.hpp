#ifndef BEAST_PIPELINE_HPP_
#define BEAST_PIPELINE_HPP_

// Standard
#include <list>
#include <memory>

// BEAST
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class Pipeline
 * @brief Work management class orchestrating `Pipe` class instances
 *
 * Pipelines hold any number of pipes and orchestrate forwarding output candidate programs to the
 * input ports of other Pipes. Pipelines also manage the underlying worker thread models. They are
 * the main vessel for sequential evolution management.
 *
 * @author Jan Winkler
 * @date 2023-03-06
 */
class Pipeline {
 public:
  /**
   * @brief Holds information about how Pipe instances are connected to each other in this Pipeline
   *
   * Pipes have a number of output and input slots. With this Connection structure, relationships
   * between Pipes in terms of slot interconnection are described.
   */
  struct Connection {
    std::shared_ptr<Pipe> source_pipe;       ///< Pipe that provides output data to the connection
    uint32_t source_slot_index;              ///< Index of the output slot of the source Pipe

    std::shared_ptr<Pipe> destination_pipe;  ///< Pipe that receives the connection on an input slot
    uint32_t destination_slot_index;         ///< Index of the input slot of the destination Pipe
  };

  /**
   * @fn Pipeline::addPipe
   * @brief Adds a Pipe instance to this Pipeline
   *
   * A Pipe that is supposed to be used inside a Pipeline needs to be added to it explicitly. Each
   * Pipe may only be added to a Pipeline once. Any Pipe should be part of only one Pipeline at any
   * point in time to avoid undefined behavior.
   *
   * @param pipe Shared pointer to the Pipe that will be added to this Pipeline instance
   */
  void addPipe(const std::shared_ptr<Pipe>& pipe);

  /**
   * @fn Pipeline::connectPipes
   * @brief Connects two Pipe instances contained inside this Pipeline
   *
   * To allow data flow between output and input ports of Pipes inside the same Pipeline, they need
   * to be connected to each other explicitly. This function allows to connect an output port of one
   * Pipe to the input port of another. These may be the same Pipe instance (which will only work if
   * the Pipe function logic allows it).
   *
   * This function will check whether both, source and destination Pipe are part of this Pipeline
   * instance, and will throw an exception if this is not the case. Also, this function will check
   * if the respective input or output ports of the Pipe instances are already occupied by other
   * connections in this Pipeline (an exception will be thrown if this is the case).
   *
   * @param source_pipe Shared pointer to the source Pipe instance
   * @param source_slot_index Index of the output slot on the source Pipe
   * @param destination_pipe Shared pointer to the destination Pipe instance
   * @param destination_slot_index Index of the input slot on the destination Pipe
   */
  void connectPipes(
      const std::shared_ptr<Pipe>& source_pipe, uint32_t source_slot_index,
      const std::shared_ptr<Pipe>& destination_pipe, uint32_t destination_slot_index);

  /**
   * @fn Pipeline::getPipes
   * @brief Returns a constant reference to the list of Pipe instances registered in this Pipeline
   *
   * @return Constant reference to the list of Pipe instances in this Pipeline
   */
  const std::list<std::shared_ptr<Pipe>>& getPipes() const;

  /**
   * @fn Pipeline::getConnections
   * @brief Returns a constant reference to the list of connections present in this Pipeline
   *
   * @return Constant reference to the list of connections present in this Pipeline
   */
  const std::list<Connection>& getConnections() const;

 private:
  /**
   * @var Pipeline::pipes_
   * @brief Holds this Pipeline's registered Pipe instances
   */
  std::list<std::shared_ptr<Pipe>> pipes_;

  /**
   * @var Pipeline::connections_
   * @brief Holds this Pipeline's connections
   */
  std::list<Connection> connections_;
};

}  // namespace beast

#endif  // BEAST_PIPELINE_HPP_
