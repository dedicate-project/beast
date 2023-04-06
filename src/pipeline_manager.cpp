#include <beast/pipeline_manager.hpp>

// Internal
#include <beast/pipes/evaluator_pipe.hpp>
#include <beast/pipes/evolution_pipe.hpp>
#include <beast/pipes/null_sink_pipe.hpp>
#include <beast/pipes/program_factory_pipe.hpp>

#include <beast/program_factory_base.hpp>
#include <beast/random_program_factory.hpp>

namespace beast {

PipelineManager::PipelineManager(const std::string& storage_path) : filesystem_(storage_path) {
  std::scoped_lock lock{pipelines_mutex_};
  for (const auto& model : filesystem_.loadModels()) {
    PipelineDescriptor descriptor;
    descriptor.id = getFreeId();
    descriptor.name = model["content"]["name"];
    descriptor.filename = model["filename"];
    descriptor.pipeline = constructPipelineFromJson(model["content"]["model"]);
    descriptor.metadata = model["content"]["metadata"];
    pipelines_.push_back(std::move(descriptor));
  }
}

uint32_t PipelineManager::createPipeline(const std::string& name) {
  std::scoped_lock lock{pipelines_mutex_};
  nlohmann::json model;
  model["pipes"] = nlohmann::json::array();
  model["connections"] = {};
  // TODO(fairlight1337): Add more data to the model.
  const std::string filename = filesystem_.saveModel(name, model);
  const uint32_t new_id = getFreeId();
  pipelines_.push_back({new_id, name, filename, beast::Pipeline()});

  return new_id;
}

void PipelineManager::savePipeline(uint32_t pipeline_id) {
  const auto& descriptor = getPipelineById(pipeline_id);
  const auto model = deconstructPipelineToJson(descriptor.pipeline);
  filesystem_.updateModel(descriptor.filename, descriptor.name, model, descriptor.metadata);
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
  pipelines_.remove_if([pipeline_id](const auto& pipeline) { return pipeline.id == pipeline_id; });
}

nlohmann::json PipelineManager::getJsonForPipeline(uint32_t pipeline_id) {
  std::scoped_lock lock{pipelines_mutex_};
  PipelineDescriptor descriptor = getPipelineById(pipeline_id);
  return deconstructPipelineToJson(descriptor.pipeline);
}

void PipelineManager::checkForParameterPresenceInPipeJson(
    const nlohmann::detail::iteration_proxy_value<nlohmann::json::basic_json::const_iterator>& json,
    const std::vector<std::string>& parameters) {
  if (parameters.empty()) {
    return;
  }
  const std::string& pipe_name = json.key();
  if (!json.value().contains("parameters")) {
    std::string error = "Parameters not defined in model configuration for pipe '";
    error += pipe_name;
    error += "'";
    throw std::invalid_argument(error);
  }
  for (const std::string& parameter : parameters) {
    if (!json.value()["parameters"].contains(parameter)) {
      std::string error = "Required parameter '";
      error += parameter;
      error += "' not defined in model configuration for pipe '";
      error += pipe_name;
      error += "'";
      throw std::invalid_argument(error);
    }
  }
}

Pipeline PipelineManager::constructPipelineFromJson(const nlohmann::json& json) {
  Pipeline pipeline;
  std::map<std::string, std::shared_ptr<Pipe>> created_pipes;
  if (json.contains("pipes") && !json["pipes"].is_null()) {
    for (const auto& pipe : json["pipes"].items()) {
      const std::string& pipe_name = pipe.key();
      if (!pipe.value().contains("type") || pipe.value()["type"].is_null()) {
        throw std::invalid_argument("Type must be defined for pipe '" + pipe_name + "'");
      }
      const std::string& pipe_type = pipe.value()["type"].get<std::string>();

      if (pipe_type == "ProgramFactoryPipe") {
        checkForParameterPresenceInPipeJson(pipe,
                                            {"factory",
                                             "max_candidates",
                                             "max_size",
                                             "memory_variables",
                                             "string_table_items",
                                             "string_table_item_length"});
        const std::string factory_type = pipe.value()["parameters"]["factory"].get<std::string>();
        std::shared_ptr<ProgramFactoryBase> factory = nullptr;
        if (factory_type == "RandomProgramFactory") {
          factory = std::make_shared<RandomProgramFactory>();
        } else {
          throw std::invalid_argument("Invalid program factory type '" + factory_type + "'");
        }
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const uint32_t max_size = pipe.value()["parameters"]["max_size"].get<uint32_t>();
        const uint32_t memory_variables =
            pipe.value()["parameters"]["memory_variables"].get<uint32_t>();
        const uint32_t string_table_items =
            pipe.value()["parameters"]["string_table_items"].get<uint32_t>();
        const uint32_t string_table_item_length =
            pipe.value()["parameters"]["string_table_item_length"].get<uint32_t>();

        created_pipes[pipe_name] = std::make_shared<ProgramFactoryPipe>(max_candidates,
                                                                        max_size,
                                                                        memory_variables,
                                                                        string_table_items,
                                                                        string_table_item_length,
                                                                        factory);
        pipeline.addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "NullSinkPipe") {
        created_pipes[pipe_name] = std::make_shared<NullSinkPipe>();
        pipeline.addPipe(pipe_name, created_pipes[pipe_name]);
      }
    }
  }
  if (json.contains("connections") && !json["connections"].is_null()) {
    for (const auto& connection : json["connections"].items()) {
      if (!connection.value().contains("source_pipe")) {
        throw std::invalid_argument("source_pipe parameter required in connection");
      }
      if (!connection.value().contains("source_slot")) {
        throw std::invalid_argument("source_slot parameter required in connection");
      }
      if (!connection.value().contains("destination_pipe")) {
        throw std::invalid_argument("destination_pipe parameter required in connection");
      }
      if (!connection.value().contains("destination_slot")) {
        throw std::invalid_argument("destination_slot parameter required in connection");
      }
      if (!connection.value().contains("buffer_size")) {
        throw std::invalid_argument("buffer_size parameter required in connection");
      }
      const std::string source_pipe = connection.value()["source_pipe"].get<std::string>();
      const uint32_t source_slot = connection.value()["source_slot"].get<uint32_t>();
      const std::string destination_pipe =
          connection.value()["destination_pipe"].get<std::string>();
      const uint32_t destination_slot = connection.value()["destination_slot"].get<uint32_t>();
      const uint32_t buffer_size = connection.value()["buffer_size"].get<uint32_t>();

      if (created_pipes.find(source_pipe) == created_pipes.end()) {
        throw std::invalid_argument("Source pipe '" + source_pipe + "' not found");
      }
      if (created_pipes.find(destination_pipe) == created_pipes.end()) {
        throw std::invalid_argument("Destination pipe '" + destination_pipe + "' not found");
      }

      pipeline.connectPipes(created_pipes[source_pipe],
                            source_slot,
                            created_pipes[destination_pipe],
                            destination_slot,
                            buffer_size);
    }
  }
  return pipeline;
}

nlohmann::json PipelineManager::deconstructPipelineToJson(const Pipeline& pipeline) {
  nlohmann::json value;

  for (const auto& pipe : pipeline.getPipes()) {
    nlohmann::json pipe_json;

    if (std::dynamic_pointer_cast<EvaluatorPipe>(pipe->pipe)) {
      pipe_json["type"] = "EvaluatorPipe";
    } else if (std::dynamic_pointer_cast<EvolutionPipe>(pipe->pipe)) {
      pipe_json["type"] = "EvolutionPipe";
    } else if (std::dynamic_pointer_cast<NullSinkPipe>(pipe->pipe)) {
      pipe_json["type"] = "NullSinkPipe";
    } else if (auto spec_pipe = std::dynamic_pointer_cast<ProgramFactoryPipe>(pipe->pipe)) {
      pipe_json["type"] = "ProgramFactoryPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["max_size"] = spec_pipe->getMaxSize();
      pipe_json["parameters"]["memory_variables"] = spec_pipe->getMemorySize();
      pipe_json["parameters"]["string_table_item_length"] = spec_pipe->getStringTableItemLength();
      pipe_json["parameters"]["string_table_items"] = spec_pipe->getStringTableSize();

      const auto factory = spec_pipe->getFactory();
      if (std::dynamic_pointer_cast<RandomProgramFactory>(factory)) {
        pipe_json["parameters"]["factory"] = "RandomProgramFactory";
      } else {
        pipe_json["parameters"]["factory"] = "Unknown";
      }
    } else {
      pipe_json["type"] = "Unknown";
    }

    value["pipes"][pipe->name] = std::move(pipe_json);
  }

  nlohmann::json connections_json = {};
  for (const auto& connection : pipeline.getConnections()) {
    nlohmann::json connection_json;
    connection_json["buffer_size"] = connection.buffer.capacity();
    connection_json["destination_pipe"] = connection.destination_pipe->name;
    connection_json["destination_slot"] = connection.destination_slot_index;
    connection_json["source_pipe"] = connection.source_pipe->name;
    connection_json["source_slot"] = connection.source_slot_index;

    connections_json.push_back(std::move(connection_json));
  }
  value["connections"] = std::move(connections_json);

  return value;
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
