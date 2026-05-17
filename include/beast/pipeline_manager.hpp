#ifndef BEAST_PIPELINE_MANAGER_HPP_
#define BEAST_PIPELINE_MANAGER_HPP_

// Standard
#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// Internal
#include <beast/filesystem_helper.hpp>
#include <beast/pipeline.hpp>
#include <beast/pipes/evolution_pipe.hpp>

#include <beast/evaluators/aggregation_evaluator.hpp>

namespace beast {

/**
 * @class PipelineManager
 * @brief Manages a collection of pipelines.
 */
class PipelineManager {
 public:
  /**
   * @struct PipelineDescriptor
   * @brief Describes a pipeline with its ID, name, filename, Pipeline object, and metadata.
   */
  struct PipelineDescriptor {
    uint32_t id;                        //!< Unique identifier for the pipeline.
    std::string name;                   //!< Display name of the pipeline.
    std::string filename;               //!< Filename of the pipeline's serialized data.
    std::shared_ptr<Pipeline> pipeline; //!< The Pipeline object.
    nlohmann::json metadata;            //!< The metadata object.
  };

  /**
   * @brief Constructor that initializes the PipelineManager with a storage path.
   * @param storage_path Path to the storage directory.
   * @param metrics_interval_time Interval time for metrics collection, in ms.
   */
  explicit PipelineManager(const std::string& storage_path, uint32_t metrics_interval_time,
                           uint32_t metrics_window_size);

  ~PipelineManager();

  /**
   * @brief Creates a new pipeline and adds it to the collection.
   * @param name The display name for the new pipeline.
   * @return The unique identifier for the new pipeline.
   */
  [[nodiscard]] uint32_t createPipeline(const std::string& name);

  void savePipeline(uint32_t pipeline_id);

  /**
   * @brief Gets a reference to a pipeline by its ID.
   *
   * @note This intentionally does NOT lock `pipelines_mutex_`; callers that need atomicity
   *       across the lookup + use must acquire the mutex themselves (see e.g.
   *       updatePipelineName, deletePipeline). The returned reference is only valid as long
   *       as that lock is held -- a concurrent delete would invalidate it.
   *
   * @param pipeline_id The unique identifier of the desired pipeline.
   * @return A reference to the pipeline descriptor with the given ID.
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  [[nodiscard]] PipelineDescriptor& getPipelineById(uint32_t pipeline_id);

  /// @copydoc getPipelineById(uint32_t)
  [[nodiscard]] const PipelineDescriptor& getPipelineById(uint32_t pipeline_id) const;

  /**
   * @brief Gets a const reference to the list of pipeline descriptors.
   * @return A const reference to the list of pipeline descriptors.
   */
  [[nodiscard]] const std::list<PipelineDescriptor>& getPipelines() const;

  /**
   * @brief Updates the name of the given pipeline.
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  void updatePipelineName(uint32_t pipeline_id, const std::string_view new_name);

  /**
   * @brief Deletes the given pipeline.
   *
   * @param pipeline_id The ID of the pipeline
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  void deletePipeline(uint32_t pipeline_id);

  /**
   * @brief Returns the metrics for the given pipeline
   *
   * @param pipeline_id The ID of the pipeline
   * @return The metrics object for the given pipeline
   */
  Pipeline::PipelineMetrics getPipelineMetrics(uint32_t pipeline_id);

  /**
   * @brief Gets the JSON representation of the specified pipeline.
   * @param pipeline_id The unique identifier of the pipeline.
   * @return The JSON representation of the pipeline.
   */
  [[nodiscard]] nlohmann::json getJsonForPipeline(uint32_t pipeline_id);

  /**
   * @brief Atomically mutate a pipeline's model + metadata.
   *
   * The mutator callback receives the current model (with `pipes` / `connections` keys) and
   * UI metadata. After it returns, the model is fed back through `constructPipelineFromJson`
   * to materialise a fresh `Pipeline` instance, and the result is swapped into the
   * descriptor and persisted to disk.
   *
   * Mutations are rejected if the target pipeline is currently running -- rebuilding the
   * Pipe graph while worker threads are iterating over it would be a data race. Callers
   * should stop the pipeline first.
   *
   * @throws std::invalid_argument if the pipeline is unknown, currently running, or if the
   *         mutated JSON fails to deserialise into a valid pipeline (e.g. unknown pipe type,
   *         missing required parameter, port collision).
   */
  void mutatePipeline(uint32_t pipeline_id,
                      const std::function<void(nlohmann::json& model,
                                               nlohmann::json& metadata)>& mutator);

  /**
   * @brief Constructs a vector of evaluator tuples from a JSON object.
   * @param json The JSON object containing the evaluator data.
   * @return A vector of tuples, each containing a shared pointer to an Evaluator, a double weight,
   * and a bool indicating if it's a minimization evaluator.
   */
  [[nodiscard]] static std::vector<std::tuple<std::shared_ptr<Evaluator>, double, bool>>
  constructEvaluatorsFromJson(const nlohmann::json& json);

  [[nodiscard]] static std::shared_ptr<Evaluator>
  constructAggregationEvaluatorFromJson(const nlohmann::json& json);

  [[nodiscard]] static std::shared_ptr<Evaluator>
  constructMazeEvaluatorFromJson(const nlohmann::json& json);

  /**
   * @brief Deserializes a JSON object into an `EvolutionPipe::EvolutionParameters` struct.
   *
   * Missing fields fall back to the C++-side defaults defined on `EvolutionParameters`, so
   * older pipeline JSON files that predate a particular knob still load cleanly.
   */
  [[nodiscard]] static EvolutionPipe::EvolutionParameters
  constructEvolutionParametersFromJson(const nlohmann::json& json);

  /**
   * @brief Serializes an `EvolutionPipe::EvolutionParameters` struct into JSON.
   *
   * The shape mirrors the struct field names and uses string keys for `opcode_weights`
   * (where the keys are the integer underlying value of `OpCode`, stringified for JSON
   * object compatibility).
   */
  [[nodiscard]] static nlohmann::json
  deconstructEvolutionParametersToJson(const EvolutionPipe::EvolutionParameters& parameters);

  /**
   * @brief Constructs a Pipeline object from a JSON object.
   * @param json The JSON object containing the pipeline data.
   * @return A Pipeline object constructed from the JSON data.
   */
  [[nodiscard]] static std::shared_ptr<Pipeline>
  constructPipelineFromJson(const nlohmann::json& json);

  /**
   * @brief Deconstructs a Pipeline object into a JSON object.
   * @param pipeline The Pipeline object to deconstruct.
   * @return A JSON object representing the Pipeline object.
   */
  [[nodiscard]] static nlohmann::json
  deconstructPipelineToJson(const std::shared_ptr<Pipeline>& pipeline);

 private:
  /**
   * @brief Checks if the specified parameters are present in the given JSON object.
   * @param json A JSON object to check for the presence of parameters.
   * @param parameters A vector of strings representing the parameter keys to look for.
   * @throws std::invalid_argument if a required parameter is missing from the JSON object.
   */
  static void checkForParameterPresenceInPipeJson(
      const nlohmann::detail::iteration_proxy_value<nlohmann::json::basic_json::const_iterator>&
          json,
      const std::vector<std::string>& parameters);

  /**
   * @brief Checks if the specified keys are present in the given JSON object.
   * @param json A JSON object to check for the presence of keys.
   * @param keys A vector of strings representing the keys to look for.
   * @throws std::invalid_argument if a required key is missing from the JSON object.
   */
  static void checkForKeyPresenceInJson(const nlohmann::json& json,
                                        const std::vector<std::string>& keys);

  /**
   * @brief Deconstructs a vector of EvaluatorDescription objects into a JSON object.
   * @param descriptions A vector of AggregationEvaluator::EvaluatorDescription objects to
   * deconstruct.
   * @return A JSON object representing the vector of EvaluatorDescription objects.
   */
  [[nodiscard]] static nlohmann::json deconstructEvaluatorsToJson(
      const std::vector<AggregationEvaluator::EvaluatorDescription>& descriptions);

  /**
   * @brief Gets an unused pipeline ID.
   * @return A unique pipeline ID that is not currently in use.
   */
  [[nodiscard]] uint32_t getFreeId() const;

  void metricsCollectorWorker();

  /**
   * @var PipelineMetrics::metrics_interval_time_
   * @brief Interval duration of one metrics collection cycle, in ms.
   */
  uint32_t metrics_interval_time_;

  double metrics_time_constant_;

  uint32_t metrics_window_size_;

  std::thread metrics_collector_thread_;

  // Read by the metrics collector thread on every loop iteration; written by the destructor on the
  // owning thread. Marked atomic so the worker is guaranteed to observe shutdown without a
  // surrounding mutex.
  std::atomic<bool> should_run_metrics_collector_{false};

  std::unordered_map<uint32_t, Pipeline::PipelineMetrics> metrics_;

  std::mutex metrics_mutex_;

  /**
   * @var PipelineManager::filesystem_
   * @brief Filesystem helper for managing pipeline storage.
   */
  FilesystemHelper filesystem_;

  /**
   * @var PipelineManager::pipelines_
   * @brief Collection of pipeline descriptors managed by the PipelineManager.
   */
  std::list<PipelineDescriptor> pipelines_;

  /**
   * @var PipelineManager::pipelines_mutex_
   * @brief Mutex for thread-safe access to the collection of pipeline descriptors.
   */
  std::mutex pipelines_mutex_;
};

} // namespace beast

#endif // BEAST_PIPELINE_MANAGER_HPP_
