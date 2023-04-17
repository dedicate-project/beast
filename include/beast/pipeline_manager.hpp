#ifndef BEAST_PIPELINE_MANAGER_HPP_
#define BEAST_PIPELINE_MANAGER_HPP_

// Standard
#include <list>
#include <mutex>
#include <string>

// Internal
#include <beast/filesystem_helper.hpp>
#include <beast/pipeline.hpp>

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
   */
  explicit PipelineManager(const std::string& storage_path);

  /**
   * @brief Creates a new pipeline and adds it to the collection.
   * @param name The display name for the new pipeline.
   * @return The unique identifier for the new pipeline.
   */
  [[nodiscard]] uint32_t createPipeline(const std::string& name);

  void savePipeline(uint32_t pipeline_id);

  /**
   * @brief Gets a reference to a pipeline by its ID.
   * @param id The unique identifier of the desired pipeline.
   * @return A reference to the pipeline descriptor with the given ID.
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  [[nodiscard]] PipelineDescriptor& getPipelineById(uint32_t pipeline_id);

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
