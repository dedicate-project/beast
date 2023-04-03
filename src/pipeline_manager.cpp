#include <beast/pipeline_manager.hpp>

namespace beast {

PipelineManager::PipelineManager(const std::string& storage_path) : filesystem_(storage_path) {
  std::scoped_lock lock{pipelines_mutex_};
  for (const auto& model : filesystem_.loadModels()) {
    PipelineDescriptor descriptor;
    descriptor.id = getFreeId();
    descriptor.name = model["content"]["name"];
    descriptor.filename = model["filename"];
    descriptor.pipeline = constructPipelineFromJson(model["content"]["model"]);
    pipelines_.push_back(std::move(descriptor));
  }
}

uint32_t PipelineManager::createPipeline(const std::string& name) {
  std::scoped_lock lock{pipelines_mutex_};
  nlohmann::json model;
  // TODO(fairlight1337): Add more data to the model.
  const std::string filename = filesystem_.saveModel(name, model);
  const uint32_t new_id = getFreeId();
  pipelines_.push_back({new_id, name, filename, beast::Pipeline()});

  return new_id;
}

PipelineManager::PipelineDescriptor& PipelineManager::getPipelineById(uint32_t pipeline_id) {
  for (PipelineDescriptor& descriptor : pipelines_) {
    if (descriptor.id == pipeline_id) {
      return descriptor;
    }
  }
  throw std::invalid_argument("Pipeline with this ID not found: " + std::to_string(pipeline_id));
}

const std::list<PipelineManager::PipelineDescriptor>& PipelineManager::getPipelines() const {
  return pipelines_;
}

void PipelineManager::updatePipelineName(uint32_t pipeline_id, const std::string_view new_name) {
  std::scoped_lock lock{pipelines_mutex_};
  PipelineDescriptor& descriptor = getPipelineById(pipeline_id);
  descriptor.name = new_name;
}

void PipelineManager::deletePipeline(uint32_t pipeline_id) {
  std::scoped_lock lock{pipelines_mutex_};
  PipelineDescriptor descriptor = getPipelineById(pipeline_id);
  filesystem_.deleteModel(descriptor.filename);
  pipelines_.remove_if([pipeline_id](auto pipeline) { return pipeline.id == pipeline_id; });
}

Pipeline PipelineManager::constructPipelineFromJson(const nlohmann::json& /*json*/) {
  // TODO(fairlight1337): Actually construct the pipeline from the data in the
  // json object. For now, only an empty pipeline object is returned until the
  // pipeline description in json is defined.
  return {};
}

uint32_t PipelineManager::getFreeId() const {
  uint32_t new_id = -1;
  std::list<PipelineDescriptor>::const_iterator iter;
  do {
    new_id++;
    iter = std::find_if(pipelines_.begin(), pipelines_.end(), [new_id](const auto& pipeline) {
      return pipeline.id == new_id;
    });
  } while (iter != pipelines_.end());
  return new_id;
}

} // namespace beast
