#include <beast/pipeline_manager.hpp>

// Standard
#include <chrono>

// Internal
#include <beast/pipes/evaluator_pipe.hpp>
#include <beast/pipes/evolution_pipe.hpp>
#include <beast/pipes/null_sink_pipe.hpp>
#include <beast/pipes/program_factory_pipe.hpp>

#include <beast/program_factory_base.hpp>
#include <beast/random_program_factory.hpp>

#include <beast/evaluators/maze_evaluator.hpp>

namespace beast {

PipelineManager::PipelineManager(const std::string& storage_path, uint32_t metrics_interval_time,
                                 uint32_t metrics_window_size)
    : filesystem_(storage_path), metrics_interval_time_{metrics_interval_time},
      metrics_window_size_{metrics_window_size} {
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

  if (metrics_interval_time == 0) {
    throw std::invalid_argument("metrics_interval_time must be > 0");
  }

  if (metrics_window_size == 0) {
    throw std::invalid_argument("metrics_window_size must be > 0");
  }

  metrics_time_constant_ =
      static_cast<uint32_t>(std::ceil(metrics_interval_time / metrics_window_size));

  should_run_metrics_collector_ = true;
  metrics_collector_thread_ = std::thread(&PipelineManager::metricsCollectorWorker, this);
}

PipelineManager::~PipelineManager() {
  should_run_metrics_collector_ = false;
  metrics_collector_thread_.join();
}

uint32_t PipelineManager::createPipeline(const std::string& name) {
  std::scoped_lock lock{pipelines_mutex_};
  nlohmann::json model;
  model["pipes"] = nlohmann::json::array();
  model["connections"] = {};
  const std::string filename = filesystem_.saveModel(name, model);
  const uint32_t new_id = getFreeId();

  PipelineDescriptor descriptor;
  descriptor.id = new_id;
  descriptor.name = name;
  descriptor.filename = filename;
  descriptor.pipeline = std::make_shared<Pipeline>();

  pipelines_.push_back(descriptor);

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

Pipeline::PipelineMetrics PipelineManager::getPipelineMetrics(uint32_t pipeline_id) {
  std::scoped_lock lock(metrics_mutex_);
  auto pipeline_metrics = metrics_.find(pipeline_id);
  if (pipeline_metrics != metrics_.end()) {
    return pipeline_metrics->second;
  }
  return Pipeline::PipelineMetrics{};
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

void PipelineManager::checkForKeyPresenceInJson(const nlohmann::json& json,
                                                const std::vector<std::string>& keys) {
  if (keys.empty()) {
    return;
  }
  for (const std::string& key : keys) {
    if (!json.contains(key)) {
      std::string error = "Required key '";
      error += key;
      error += "' not defined";
      throw std::invalid_argument(error);
    }
  }
}

std::vector<std::tuple<std::shared_ptr<Evaluator>, double, bool>>
PipelineManager::constructEvaluatorsFromJson(const nlohmann::json& json) {
  std::vector<std::tuple<std::shared_ptr<Evaluator>, double, bool>> evaluators;
  for (const auto& evaluator_json : json.items()) {
    checkForKeyPresenceInJson(evaluator_json.value(), {"type", "weight", "invert_logic"});
    const std::string type = evaluator_json.value()["type"].get<std::string>();

    std::shared_ptr<Evaluator> evaluator = nullptr;
    double weight = 0;
    bool invert_logic = false;

    if (type == "AggregationEvaluator") {
      evaluator = constructAggregationEvaluatorFromJson(evaluator_json.value());
      weight = evaluator_json.value()["weight"].get<double>();
      invert_logic = evaluator_json.value()["invert_logic"].get<bool>();
    } else if (type == "MazeEvaluator") {
      evaluator = constructMazeEvaluatorFromJson(evaluator_json.value());
      weight = evaluator_json.value()["weight"].get<double>();
      invert_logic = evaluator_json.value()["invert_logic"].get<bool>();
    } else {
      throw std::invalid_argument("Invalid evaluator type: " + type);
    }

    evaluators.emplace_back(evaluator, weight, invert_logic);
  }
  return evaluators;
}

std::shared_ptr<Evaluator>
PipelineManager::constructAggregationEvaluatorFromJson(const nlohmann::json& json) {
  auto evaluator = std::make_shared<AggregationEvaluator>();
  if (json.contains("parameters") && json["parameters"].contains("evaluators")) {
    const auto evaluator_triplets = constructEvaluatorsFromJson(json["parameters"]["evaluators"]);
    for (const auto& [sub_evaluator, weight, invert_logic] : evaluator_triplets) {
      std::dynamic_pointer_cast<AggregationEvaluator>(evaluator)->addEvaluator(
          sub_evaluator, weight, invert_logic);
    }
  }
  return evaluator;
}

std::shared_ptr<Evaluator>
PipelineManager::constructMazeEvaluatorFromJson(const nlohmann::json& json) {
  checkForKeyPresenceInJson(json, {"parameters"});
  checkForKeyPresenceInJson(json["parameters"], {"rows", "cols", "difficulty", "max_steps"});
  const uint32_t rows = json["parameters"]["rows"].get<uint32_t>();
  const uint32_t cols = json["parameters"]["cols"].get<uint32_t>();
  const double difficulty = json["parameters"]["difficulty"].get<double>();
  const uint32_t max_steps = json["parameters"]["max_steps"].get<uint32_t>();
  return std::make_shared<MazeEvaluator>(rows, cols, difficulty, max_steps);
}

std::shared_ptr<Pipeline> PipelineManager::constructPipelineFromJson(const nlohmann::json& json) {
  std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
  std::map<std::string, std::shared_ptr<Pipe>, std::less<>> created_pipes;
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
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const uint32_t max_size = pipe.value()["parameters"]["max_size"].get<uint32_t>();
        const uint32_t memory_variables =
            pipe.value()["parameters"]["memory_variables"].get<uint32_t>();
        const uint32_t string_table_items =
            pipe.value()["parameters"]["string_table_items"].get<uint32_t>();
        const uint32_t string_table_item_length =
            pipe.value()["parameters"]["string_table_item_length"].get<uint32_t>();

        const std::string factory_type = pipe.value()["parameters"]["factory"].get<std::string>();
        std::shared_ptr<ProgramFactoryBase> factory = nullptr;
        if (factory_type == "RandomProgramFactory") {
          factory = std::make_shared<RandomProgramFactory>();
        } else {
          throw std::invalid_argument("Invalid program factory type '" + factory_type + "'");
        }

        created_pipes[pipe_name] = std::make_shared<ProgramFactoryPipe>(max_candidates,
                                                                        max_size,
                                                                        memory_variables,
                                                                        string_table_items,
                                                                        string_table_item_length,
                                                                        factory);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "NullSinkPipe") {
        created_pipes[pipe_name] = std::make_shared<NullSinkPipe>();
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "EvaluatorPipe") {
        checkForParameterPresenceInPipeJson(pipe,
                                            {"evaluators",
                                             "max_candidates",
                                             "memory_variables",
                                             "string_table_items",
                                             "string_table_item_length"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const uint32_t memory_variables =
            pipe.value()["parameters"]["memory_variables"].get<uint32_t>();
        const uint32_t string_table_items =
            pipe.value()["parameters"]["string_table_items"].get<uint32_t>();
        const uint32_t string_table_item_length =
            pipe.value()["parameters"]["string_table_item_length"].get<uint32_t>();

        auto evaluator_pipe = std::make_shared<EvaluatorPipe>(
            max_candidates, memory_variables, string_table_items, string_table_item_length);
        created_pipes[pipe_name] = evaluator_pipe;

        const auto evaluator_triplets =
            constructEvaluatorsFromJson(pipe.value()["parameters"]["evaluators"]);
        for (const auto& evaluator_triplet : evaluator_triplets) {
          std::shared_ptr<Evaluator> evaluator = nullptr;
          double weight = 0.0;
          bool invert_logic = false;
          std::tie(evaluator, weight, invert_logic) = evaluator_triplet;
          evaluator_pipe->addEvaluator(evaluator, weight, invert_logic);
        }

        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
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

      pipeline->connectPipes(created_pipes[source_pipe],
                             source_slot,
                             created_pipes[destination_pipe],
                             destination_slot,
                             buffer_size);
    }
  }
  return pipeline;
}

nlohmann::json PipelineManager::deconstructEvaluatorsToJson(
    const std::vector<AggregationEvaluator::EvaluatorDescription>& descriptions) {
  nlohmann::json evaluators = {};

  for (const auto& description : descriptions) {
    nlohmann::json evaluator = nlohmann::json::object();
    evaluator["weight"] = description.weight;
    evaluator["invert_logic"] = description.invert_logic;

    if (const auto aggr_eval =
            std::dynamic_pointer_cast<AggregationEvaluator>(description.evaluator)) {
      evaluator["type"] = "AggregationEvaluator";
      evaluator["parameters"]["evaluators"] =
          deconstructEvaluatorsToJson(aggr_eval->getEvaluators());
    } else if (const auto maze_eval =
                   std::dynamic_pointer_cast<MazeEvaluator>(description.evaluator)) {
      evaluator["type"] = "MazeEvaluator";
      evaluator["parameters"]["rows"] = maze_eval->getRows();
      evaluator["parameters"]["cols"] = maze_eval->getCols();
      evaluator["parameters"]["difficulty"] = maze_eval->getDifficulty();
      evaluator["parameters"]["max_steps"] = maze_eval->getMaxSteps();
    }

    evaluators.push_back(std::move(evaluator));
  }

  return evaluators;
}

nlohmann::json
PipelineManager::deconstructPipelineToJson(const std::shared_ptr<Pipeline>& pipeline) {
  nlohmann::json value;

  for (const auto& pipe : pipeline->getPipes()) {
    nlohmann::json pipe_json;

    if (const auto evaluator_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(pipe->pipe)) {
      pipe_json["type"] = "EvaluatorPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["memory_variables"] = evaluator_pipe->getMemorySize();
      pipe_json["parameters"]["string_table_item_length"] =
          evaluator_pipe->getStringTableItemLength();
      pipe_json["parameters"]["string_table_items"] = evaluator_pipe->getStringTableSize();
      pipe_json["parameters"]["evaluators"] =
          deconstructEvaluatorsToJson(evaluator_pipe->getEvaluators());
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
  for (const auto& connection : pipeline->getConnections()) {
    nlohmann::json connection_json;
    connection_json["buffer_size"] = connection->buffer_size;
    connection_json["destination_pipe"] = connection->destination_pipe->name;
    connection_json["destination_slot"] = connection->destination_slot_index;
    connection_json["source_pipe"] = connection->source_pipe->name;
    connection_json["source_slot"] = connection->source_slot_index;

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

void PipelineManager::metricsCollectorWorker() {
  std::chrono::milliseconds interval_time(metrics_interval_time_);

  std::unordered_map<uint32_t, std::deque<Pipeline::PipelineMetrics>> metrics_cache;
  while (should_run_metrics_collector_) {
    // Remove the oldest element from the cache for each pipeline if maximum window size is reached.
    for (auto& metrics_pair : metrics_cache) {
      if (metrics_pair.second.size() >= metrics_window_size_) {
        metrics_pair.second.pop_front();
      }
    }

    // Collect the current metrics and push them into the cache.
    {
      std::scoped_lock lock(pipelines_mutex_);
      for (auto& descriptor : pipelines_) {
        metrics_cache[descriptor.id].push_back(descriptor.pipeline->getMetrics());
      }
    }

    // Calculate the current metrics.
    const auto now = std::chrono::system_clock::now();
    std::unordered_map<uint32_t, Pipeline::PipelineMetrics> metrics;
    for (auto& metrics_pair : metrics_cache) {
      const uint32_t pipeline_id = metrics_pair.first;
      const std::deque<Pipeline::PipelineMetrics>& metrics_history = metrics_pair.second;

      // Calculate the weighted average for each metric in the pipeline.
      std::unordered_map<std::string, Pipeline::PipeMetrics> pipe_metrics;
      double sum_weights = 0.0; // Initialize the sum of weights
      for (const auto& pipeline_metrics : metrics_history) {
        std::chrono::duration<double> elapsed_time = now - pipeline_metrics.measure_time_start;
        const double weight = std::exp(-elapsed_time.count() / metrics_time_constant_);
        sum_weights += weight; // Add the current weight to the sum of weights
        double duration_seconds = elapsed_time.count();

        for (const auto& pipe_pair : pipeline_metrics.pipes) {
          const std::string pipe_name = pipe_pair.first;
          const Pipeline::PipeMetrics& current_pipe_metrics = pipe_pair.second;
          Pipeline::PipeMetrics& current_weighted_metrics = pipe_metrics[pipe_name];

          // Update the execution count.
          current_weighted_metrics.execution_count +=
              weight * current_pipe_metrics.execution_count / duration_seconds;

          // Update the input and output maps.
          for (const auto& input_pair : current_pipe_metrics.inputs_received) {
            const uint32_t input_id = input_pair.first;
            const uint32_t input_count = input_pair.second;
            current_weighted_metrics.inputs_received[input_id] +=
                weight * input_count / duration_seconds;
          }
          for (const auto& output_pair : current_pipe_metrics.outputs_sent) {
            const uint32_t output_id = output_pair.first;
            const uint32_t output_count = output_pair.second;
            current_weighted_metrics.outputs_sent[output_id] +=
                weight * output_count / duration_seconds;
          }
        }
      }

      // Normalize the accumulated values by the sum of weights to obtain the weighted average.
      for (auto& pipe_pair : pipe_metrics) {
        Pipeline::PipeMetrics& weighted_metrics = pipe_pair.second;
        weighted_metrics.execution_count /= sum_weights;

        for (auto& input_pair : weighted_metrics.inputs_received) {
          input_pair.second /= sum_weights;
        }
        for (auto& output_pair : weighted_metrics.outputs_sent) {
          output_pair.second /= sum_weights;
        }
      }

      // Store the resulting metrics for the pipeline.
      Pipeline::PipelineMetrics& pipeline_metrics = metrics[pipeline_id];
      pipeline_metrics.measure_time_start = metrics_history.back().measure_time_start;
      pipeline_metrics.pipes = pipe_metrics;
    }

    // Store the resulting metrics.
    {
      std::scoped_lock lock(metrics_mutex_);
      metrics_ = metrics;
    }

    // Limit cycle time.
    std::this_thread::sleep_for(interval_time);
  }
}

} // namespace beast
