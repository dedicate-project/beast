#ifndef BEAST_PIPELINE_HPP_
#define BEAST_PIPELINE_HPP_

// Standard
#include <condition_variable>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

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
  struct ManagedPipe {
    std::string name;
    std::shared_ptr<Pipe> pipe;
    std::thread thread;
    bool should_run;
    bool is_running;
  };

  /**
   * @brief Holds information about how Pipe instances are connected to each other in this Pipeline
   *
   * Pipes have a number of output and input slots. With this Connection structure, relationships
   * between Pipes in terms of slot interconnection are described.
   */
  struct Connection {
    std::shared_ptr<ManagedPipe> source_pipe; ///< Pipe that provides output data to the connection
    uint32_t source_slot_index;               ///< Index of the output slot of the source Pipe

    std::shared_ptr<ManagedPipe>
        destination_pipe;            ///< Pipe that receives the connection on an input slot
    uint32_t destination_slot_index; ///< Index of the input slot of the destination Pipe

    std::deque<Pipe::OutputItem> buffer; ///< Data buffer from source to destination
    uint32_t buffer_size;                ///< The size of the data buffer
    std::mutex buffer_mutex;
  };

  /**
   * @brief Holds metrics describing the performance of an individual pipe
   */
  struct PipeMetrics {
    uint32_t execution_count;
    std::unordered_map<uint32_t, uint32_t> inputs_received;
    std::unordered_map<uint32_t, uint32_t> outputs_sent;
  };

  /**
   * @brief Holds metrics describing the performance of all pipes in this Pipeline instance
   */
  struct PipelineMetrics {
    std::unordered_map<std::string, PipeMetrics> pipes;
    std::chrono::time_point<std::chrono::steady_clock> measure_time_start;
  };

  Pipeline();

  /**
   * @fn Pipeline::addPipe
   * @brief Adds a Pipe instance to this Pipeline
   *
   * A Pipe that is supposed to be used inside a Pipeline needs to be added to it explicitly. Each
   * Pipe may only be added to a Pipeline once. Any Pipe should be part of only one Pipeline at any
   * point in time to avoid undefined behavior.
   *
   * @param name A unique string identifier for this pipe
   * @param pipe Shared pointer to the Pipe that will be added to this Pipeline instance
   */
  void addPipe(const std::string& name, const std::shared_ptr<Pipe>& pipe);

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
   * During operation, incoming candidate programs from the source pipe will be stored in the
   * internal buffer. Its size is defined by the buffer_size parameter. Source pipes are halted when
   * the buffer reaches its limit until there is more space available. Destination pipes draw
   * candidates from the buffer as their own input and will be halted if not enough data is
   * available.
   *
   * @param source_pipe Shared pointer to the source Pipe instance
   * @param source_slot_index Index of the output slot on the source Pipe
   * @param destination_pipe Shared pointer to the destination Pipe instance
   * @param destination_slot_index Index of the input slot on the destination Pipe
   * @param buffer_size The size of the pipe's buffer space
   */
  void connectPipes(const std::shared_ptr<Pipe>& source_pipe, uint32_t source_slot_index,
                    const std::shared_ptr<Pipe>& destination_pipe, uint32_t destination_slot_index,
                    uint32_t buffer_size);

  /**
   * @fn Pipeline::getPipes
   * @brief Returns a constant reference to the list of Pipe instances registered in this Pipeline
   *
   * @return Constant reference to the list of Pipe instances in this Pipeline
   */
  [[nodiscard]] const std::list<std::shared_ptr<ManagedPipe>>& getPipes() const;

  /**
   * @fn Pipeline::getConnections
   * @brief Returns a constant reference to the list of connections present in this Pipeline
   *
   * @return Constant reference to the list of connections present in this Pipeline
   */
  [[nodiscard]] const std::list<std::shared_ptr<Connection>>& getConnections() const;

  /**
   * @fn Pipeline::start
   * @brief Starts this pipeline
   *
   * Throws an exception is called when the pipeline is already running.
   */
  void start();

  /**
   * @fn Pipeline::stop
   * @brief Stops this pipeline
   *
   * Throws an exception is called when the pipeline is not running.
   */
  void stop();

  /**
   * @fn Pipeline::isRunning
   * @brief Denotes whether this pipeline is currently running
   *
   * @return Boolean value denoting whether this pipeline is currently running
   */
  [[nodiscard]] bool isRunning() const;

  /**
   * @fn Pipeline::getMetrics
   * @brief Returns the current metrics data for this pipeline
   *
   * This function also resets the internal metrics counter and measurement start time. Call this
   * function in an appropriate timeframe to average consistently (e.g., every 100ms).
   *
   * @return The metrics object describing this pipeline's performance
   */
  [[nodiscard]] PipelineMetrics getMetrics();

 private:
  /**
   * @fn Pipeline::pipeIsInPipeline
   * @brief Checks if the given pipe is already part of this pipeline
   *
   * @param pipe Shared pointer to the Pipe instance to be checked
   * @return Boolean value indicating whether the pipe is part of this pipeline
   */
  [[nodiscard]] bool pipeIsInPipeline(const std::shared_ptr<Pipe>& pipe) const;

  /**
   * @fn Pipeline::findConnections
   * @brief Populates source and destination connection lists for a given managed pipe
   *
   * @param managed_pipe Shared pointer to the ManagedPipe instance
   * @param source_connections Vector of source connections to be populated
   * @param destination_connections Vector of destination connections to be populated
   */
  void findConnections(const std::shared_ptr<ManagedPipe>& managed_pipe,
                       std::vector<std::shared_ptr<Connection>>& source_connections,
                       std::vector<std::shared_ptr<Connection>>& destination_connections) const;

  /**
   * @fn Pipeline::processOutputSlots
   * @brief Processes output slots for a given managed pipe
   *
   * @param managed_pipe Shared pointer to the ManagedPipe instance
   * @param destination_connections Vector of destination connections for the managed pipe
   * @param Metrics describing how many items were moved per slot index
   */
  [[nodiscard]] static std::unordered_map<uint32_t, uint32_t>
  processOutputSlots(const std::shared_ptr<ManagedPipe>& managed_pipe,
                     const std::vector<std::shared_ptr<Connection>>& destination_connections);

  /**
   * @fn Pipeline::processInputSlots
   * @brief Processes input slots for a given managed pipe
   *
   * @param managed_pipe Shared pointer to the ManagedPipe instance
   * @param source_connections Vector of source connections for the managed pipe
   * @param Metrics describing how many items were moved per slot index
   */
  [[nodiscard]] static std::unordered_map<uint32_t, uint32_t>
  processInputSlots(const std::shared_ptr<ManagedPipe>& managed_pipe,
                    const std::vector<std::shared_ptr<Connection>>& source_connections);

  /**
   * @fn Pipeline::pipelineWorker
   * @brief Worker function responsible for processing input and output slots for a managed pipe
   *
   * @param managed_pipe Shared pointer to the ManagedPipe instance
   */
  void pipelineWorker(const std::shared_ptr<ManagedPipe>& managed_pipe);

  [[nodiscard]] std::shared_ptr<ManagedPipe>
  getManagedPipeForPipe(const std::shared_ptr<Pipe>& pipe) const;

  [[nodiscard]] std::shared_ptr<ManagedPipe> getManagedPipeByName(std::string_view name) const;

  void reportMetrics(const std::shared_ptr<ManagedPipe>& managed_pipe, bool executed,
                     const std::unordered_map<uint32_t, uint32_t>& input_metrics,
                     const std::unordered_map<uint32_t, uint32_t>& output_metrics);

  PipelineMetrics metrics_;

  std::mutex metrics_mutex_;

  /**
   * @var Pipeline::pipes_
   * @brief Holds this Pipeline's registered Pipe instances
   */
  std::list<std::shared_ptr<ManagedPipe>> pipes_;

  /**
   * @var Pipeline::connections_
   * @brief Holds this Pipeline's connections
   */
  std::list<std::shared_ptr<Connection>> connections_;

  /**
   * @var Pipeline::is_running_
   * @brief Holds a boolean status denoting whether this pipeline is currently running or not
   */
  bool is_running_ = false;
};

} // namespace beast

#endif // BEAST_PIPELINE_HPP_
