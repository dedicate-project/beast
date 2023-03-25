#ifndef BEAST_PIPELINE_MANAGER_HPP_
#define BEAST_PIPELINE_MANAGER_HPP_

// Standard
#include <string>
#include <list>

// Internal
#include <beast/filesystem_helper.hpp>
#include <beast/pipeline.hpp>

namespace beast {

class PipelineManager {
 public:
  struct PipelineDescriptor {
    uint32_t id;
    std::string name;
    std::string filename;
    Pipeline pipeline;
  };

  PipelineManager(const std::string& storage_path);

  uint32_t createPipeline(const std::string& name);

  PipelineDescriptor& getPipelineById(uint32_t id);

  const std::list<PipelineDescriptor>& getPipelines() const;

 private:
  static Pipeline constructPipelineFromJson(const nlohmann::json& json);

  uint32_t getFreeId() const;

  FilesystemHelper filesystem_;

  std::list<PipelineDescriptor> pipelines_;
};

}  // namespace beast

#endif  // BEAST_PIPELINE_MANAGER_HPP_
