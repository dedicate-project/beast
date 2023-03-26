#ifndef BEAST_PIPELINE_MANAGER_HPP_
#define BEAST_PIPELINE_MANAGER_HPP_

// Standard
#include <string>
#include <list>
#include <mutex>

// Internal
#include <beast/filesystem_helper.hpp>
#include <beast/pipeline.hpp>

namespace beast {

/**
 * @class PipelineManager
 * @brief Manages a collection of pipelines.
 */
class PipelineManager {
 public:
  /**
   * @struct PipelineDescriptor
   * @brief Describes a pipeline with its ID, name, filename, and Pipeline object.
   */
  struct PipelineDescriptor {
    uint32_t id;          //!< Unique identifier for the pipeline.
    std::string name;     //!< Display name of the pipeline.
    std::string filename; //!< Filename of the pipeline's serialized data.
    Pipeline pipeline;    //!< The Pipeline object.
  };

  /**
   * @brief Constructor that initializes the PipelineManager with a storage path.
   * @param storage_path Path to the storage directory.
   */
  PipelineManager(const std::string& storage_path);

  /**
   * @brief Creates a new pipeline and adds it to the collection.
   * @param name The display name for the new pipeline.
   * @return The unique identifier for the new pipeline.
   */
  uint32_t createPipeline(const std::string& name);

  /**
   * @brief Gets a reference to a pipeline by its ID.
   * @param id The unique identifier of the desired pipeline.
   * @return A reference to the pipeline descriptor with the given ID.
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  PipelineDescriptor& getPipelineById(uint32_t id);

  /**
   * @brief Gets a const reference to the list of pipeline descriptors.
   * @return A const reference to the list of pipeline descriptors.
   */
  const std::list<PipelineDescriptor>& getPipelines() const;

  /**
   * @brief Updates the name of the given pipeline.
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  void updatePipelineName(uint32_t id, const std::string& new_name);

  /**
   * @brief Deletes the given pipeline.
   * @throws std::invalid_argument if the pipeline with the given ID is not found.
   */
  void deletePipeline(uint32_t id);

 private:
  static Pipeline constructPipelineFromJson(const nlohmann::json& json);

  uint32_t getFreeId() const;

  FilesystemHelper filesystem_; //!< Filesystem helper for managing model storage.

  std::list<PipelineDescriptor> pipelines_; //!< Collection of pipelines.

  std::mutex pipelines_mutex_;
};

}  // namespace beast

#endif  // BEAST_PIPELINE_MANAGER_HPP_
