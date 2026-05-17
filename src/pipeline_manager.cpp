#include <beast/pipeline_manager.hpp>

// Standard
#include <chrono>
#include <unordered_map>

// Internal
#include <beast/evaluators/adder_evaluator.hpp>
#include <beast/evaluators/maximum_evaluator.hpp>
#include <beast/evaluators/sha256_round_evaluator.hpp>
#include <beast/pipes/demultiplexer_pipe.hpp>
#include <beast/pipes/evaluator_pipe.hpp>
#include <beast/pipes/evolution_pipe.hpp>
#include <beast/pipes/fan_pipe.hpp>
#include <beast/pipes/multiplexer_pipe.hpp>
#include <beast/pipes/null_sink_pipe.hpp>
#include <beast/pipes/program_factory_pipe.hpp>
#include <beast/pipes/program_storage_sink_pipe.hpp>
#include <beast/pipes/program_storage_source_pipe.hpp>
#include <beast/pipes/results_summary_pipe.hpp>

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

  should_run_metrics_collector_.store(true, std::memory_order_release);
  metrics_collector_thread_ = std::thread(&PipelineManager::metricsCollectorWorker, this);
}

PipelineManager::~PipelineManager() {
  should_run_metrics_collector_.store(false, std::memory_order_release);
  if (metrics_collector_thread_.joinable()) {
    metrics_collector_thread_.join();
  }

  // Stop any pipelines that are still running so worker threads are joined while *we* still
  // hold the only shared_ptr to them. Without this, ~Pipeline() would also do the right thing
  // (see Pipeline.cpp), but stopping here means the manager controls shutdown order and the
  // process can exit cleanly even on SIGINT-driven destruction paths. We swallow exceptions
  // because letting one escape a destructor is undefined behaviour.
  std::scoped_lock lock{pipelines_mutex_};
  for (auto& descriptor : pipelines_) {
    if (descriptor.pipeline && descriptor.pipeline->isRunning()) {
      try {
        descriptor.pipeline->stop();
      } catch (...) {
        // Best-effort shutdown; ~Pipeline() will still clean up any joinable workers.
      }
    }
  }
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
  std::scoped_lock lock{pipelines_mutex_};
  const auto& descriptor = getPipelineById(pipeline_id);
  const auto model = deconstructPipelineToJson(descriptor.pipeline);
  filesystem_.updateModel(descriptor.filename, descriptor.name, model, descriptor.metadata);
}

PipelineManager::PipelineDescriptor& PipelineManager::getPipelineById(uint32_t pipeline_id) {
  // Defer to the const overload to share the lookup logic without code duplication; the
  // const_cast is safe because we only call this on a non-const `*this`.
  return const_cast<PipelineDescriptor&>(
      const_cast<const PipelineManager*>(this)->getPipelineById(pipeline_id));
}

const PipelineManager::PipelineDescriptor&
PipelineManager::getPipelineById(uint32_t pipeline_id) const {
  for (const PipelineDescriptor& descriptor : pipelines_) {
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
  // Copy out only the filename (cheap std::string); the previous PipelineDescriptor-by-value
  // copy here pulled the whole metadata json along with it, which is wasteful and only worked
  // because PipelineDescriptor::pipeline is a shared_ptr.
  const std::string filename = getPipelineById(pipeline_id).filename;
  filesystem_.deleteModel(filename);
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
  // Take a const reference into the stored descriptor instead of copying the entire
  // PipelineDescriptor (which would clone the metadata json byte-for-byte).
  const PipelineDescriptor& descriptor = getPipelineById(pipeline_id);
  return deconstructPipelineToJson(descriptor.pipeline);
}

void PipelineManager::mutatePipeline(
    uint32_t pipeline_id,
    const std::function<void(nlohmann::json& model, nlohmann::json& metadata)>& mutator) {
  std::scoped_lock lock{pipelines_mutex_};
  PipelineDescriptor& descriptor = getPipelineById(pipeline_id);
  if (descriptor.pipeline && descriptor.pipeline->isRunning()) {
    throw std::invalid_argument(
        "Cannot mutate a running pipeline; stop it first before editing its structure.");
  }

  // Snapshot the current state, run the mutator on the snapshot, then validate by re-building
  // the live Pipeline from the resulting JSON. We do this on copies so a mid-mutation throw
  // (validation failure, malformed parameters, ...) leaves the descriptor untouched.
  nlohmann::json model = deconstructPipelineToJson(descriptor.pipeline);
  nlohmann::json metadata = descriptor.metadata.is_null() ? nlohmann::json::object()
                                                          : descriptor.metadata;
  mutator(model, metadata);

  // constructPipelineFromJson throws std::invalid_argument on schema problems; we let those
  // bubble up so the HTTP layer can surface the message to the user. If construction
  // succeeds, we commit by swapping in the new pipeline and persisting.
  std::shared_ptr<Pipeline> rebuilt = constructPipelineFromJson(model);

  // Carry running statistics from the old pipeline into the new one for pipe types that
  // accumulate state across cycles. Without this, every parameter edit (e.g. bumping the
  // maze evaluator's difficulty) would reset every ResultsSummaryPipe's best-ever score
  // back to 0, even when the user only touched an unrelated knob. We migrate by
  // (name, type) pair so renaming the pipe correctly resets its state -- a rename is the
  // user telling us "this is a different stage now".
  if (descriptor.pipeline) {
    std::unordered_map<std::string, std::shared_ptr<ResultsSummaryPipe>> old_summaries;
    for (const auto& managed : descriptor.pipeline->getPipes()) {
      if (auto summary = std::dynamic_pointer_cast<ResultsSummaryPipe>(managed->pipe)) {
        old_summaries.emplace(managed->name, summary);
      }
    }
    for (const auto& managed : rebuilt->getPipes()) {
      if (auto summary = std::dynamic_pointer_cast<ResultsSummaryPipe>(managed->pipe)) {
        auto previous = old_summaries.find(managed->name);
        if (previous != old_summaries.end()) {
          summary->importState(previous->second->exportState());
        }
      }
    }
  }

  descriptor.pipeline = std::move(rebuilt);
  descriptor.metadata = std::move(metadata);
  filesystem_.updateModel(descriptor.filename,
                          descriptor.name,
                          deconstructPipelineToJson(descriptor.pipeline),
                          descriptor.metadata);
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
    } else if (type == "AdderEvaluator") {
      checkForKeyPresenceInJson(evaluator_json.value(), {"parameters"});
      const auto& params = evaluator_json.value()["parameters"];
      checkForKeyPresenceInJson(params, {"trial_count", "value_range", "max_steps_per_trial"});
      evaluator = std::make_shared<AdderEvaluator>(
          params["trial_count"].get<uint32_t>(),
          params["value_range"].get<int32_t>(),
          params["max_steps_per_trial"].get<uint32_t>());
      weight = evaluator_json.value()["weight"].get<double>();
      invert_logic = evaluator_json.value()["invert_logic"].get<bool>();
    } else if (type == "MaximumEvaluator") {
      checkForKeyPresenceInJson(evaluator_json.value(), {"parameters"});
      const auto& params = evaluator_json.value()["parameters"];
      checkForKeyPresenceInJson(params,
                                {"input_count", "trial_count", "value_range",
                                 "max_steps_per_trial"});
      evaluator = std::make_shared<MaximumEvaluator>(
          params["input_count"].get<uint32_t>(),
          params["trial_count"].get<uint32_t>(),
          params["value_range"].get<int32_t>(),
          params["max_steps_per_trial"].get<uint32_t>());
      weight = evaluator_json.value()["weight"].get<double>();
      invert_logic = evaluator_json.value()["invert_logic"].get<bool>();
    } else if (type == "Sha256RoundEvaluator") {
      checkForKeyPresenceInJson(evaluator_json.value(), {"parameters"});
      const auto& params = evaluator_json.value()["parameters"];
      checkForKeyPresenceInJson(
          params, {"trial_count", "round_constant_index", "max_steps_per_trial"});
      evaluator = std::make_shared<Sha256RoundEvaluator>(
          params["trial_count"].get<uint32_t>(),
          params["round_constant_index"].get<uint32_t>(),
          params["max_steps_per_trial"].get<uint32_t>());
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

EvolutionPipe::EvolutionParameters
PipelineManager::constructEvolutionParametersFromJson(const nlohmann::json& json) {
  EvolutionPipe::EvolutionParameters parameters;
  if (json.contains("generations")) {
    parameters.generations = json["generations"].get<uint32_t>();
  }
  if (json.contains("crossover_probability")) {
    parameters.crossover_probability = json["crossover_probability"].get<double>();
  }
  if (json.contains("mutation_probability")) {
    parameters.mutation_probability = json["mutation_probability"].get<double>();
  }
  if (json.contains("elitism")) {
    parameters.elitism = json["elitism"].get<bool>();
  }
  if (json.contains("byte_mutation_share")) {
    parameters.byte_mutation_share = json["byte_mutation_share"].get<double>();
  }
  if (json.contains("variable_count")) {
    parameters.variable_count = json["variable_count"].get<uint32_t>();
  }
  if (json.contains("string_table_size")) {
    parameters.string_table_size = json["string_table_size"].get<uint32_t>();
  }
  if (json.contains("string_table_item_length")) {
    parameters.string_table_item_length = json["string_table_item_length"].get<uint32_t>();
  }
  if (json.contains("max_genome_bytes")) {
    parameters.max_genome_bytes = json["max_genome_bytes"].get<uint32_t>();
  }
  if (json.contains("starting_program_size")) {
    parameters.starting_program_size = json["starting_program_size"].get<uint32_t>();
  }
  if (json.contains("opcode_weights") && json["opcode_weights"].is_object()) {
    for (auto it = json["opcode_weights"].begin(); it != json["opcode_weights"].end(); ++it) {
      // Keys are stringified opcode integer values. Parse defensively: any non-numeric or
      // out-of-range key is silently ignored to keep load-from-disk robust against external
      // edits.
      try {
        const int raw_code = std::stoi(it.key());
        if (raw_code < 0 || raw_code >= static_cast<int>(OpCode::Size)) {
          continue;
        }
        parameters.opcode_weights[static_cast<OpCode>(raw_code)] = it.value().get<double>();
      } catch (const std::exception&) {
        // Skip malformed entry.
      }
    }
  }
  return parameters;
}

nlohmann::json PipelineManager::deconstructEvolutionParametersToJson(
    const EvolutionPipe::EvolutionParameters& parameters) {
  nlohmann::json json;
  json["generations"] = parameters.generations;
  json["crossover_probability"] = parameters.crossover_probability;
  json["mutation_probability"] = parameters.mutation_probability;
  json["elitism"] = parameters.elitism;
  json["byte_mutation_share"] = parameters.byte_mutation_share;
  json["variable_count"] = parameters.variable_count;
  json["string_table_size"] = parameters.string_table_size;
  json["string_table_item_length"] = parameters.string_table_item_length;
  json["max_genome_bytes"] = parameters.max_genome_bytes;
  json["starting_program_size"] = parameters.starting_program_size;
  nlohmann::json weights = nlohmann::json::object();
  for (const auto& [opcode, weight] : parameters.opcode_weights) {
    weights[std::to_string(static_cast<int>(opcode))] = weight;
  }
  json["opcode_weights"] = weights;
  return json;
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
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        created_pipes[pipe_name] = std::make_shared<NullSinkPipe>(max_candidates);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "ResultsSummaryPipe") {
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        // `window_size` is optional; ResultsSummaryPipe falls back to its built-in default
        // (256) when 0 is passed.
        const uint32_t window_size =
            pipe.value()["parameters"].value("window_size", static_cast<uint32_t>(0));
        created_pipes[pipe_name] =
            std::make_shared<ResultsSummaryPipe>(max_candidates, window_size);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "MultiplexerPipe") {
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates", "input_slots"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const uint32_t input_slots =
            pipe.value()["parameters"]["input_slots"].get<uint32_t>();
        created_pipes[pipe_name] = std::make_shared<MultiplexerPipe>(max_candidates,
                                                                      input_slots);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "ProgramStorageSinkPipe") {
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const std::string path =
            pipe.value()["parameters"].value("path", std::string{});
        const uint32_t top_k =
            pipe.value()["parameters"].value("top_k", static_cast<uint32_t>(0));
        created_pipes[pipe_name] =
            std::make_shared<ProgramStorageSinkPipe>(max_candidates, path, top_k);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "ProgramStorageSourcePipe") {
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const std::string path =
            pipe.value()["parameters"].value("path", std::string{});
        const bool loop = pipe.value()["parameters"].value("loop", false);
        created_pipes[pipe_name] =
            std::make_shared<ProgramStorageSourcePipe>(max_candidates, path, loop);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "FanPipe") {
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const double window_seconds =
            pipe.value()["parameters"].value("window_seconds", 0.0);
        created_pipes[pipe_name] = std::make_shared<FanPipe>(max_candidates, window_seconds);
        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else if (pipe_type == "DemultiplexerPipe") {
        checkForParameterPresenceInPipeJson(pipe, {"max_candidates", "output_slots"});
        const uint32_t max_candidates =
            pipe.value()["parameters"]["max_candidates"].get<uint32_t>();
        const uint32_t output_slots =
            pipe.value()["parameters"]["output_slots"].get<uint32_t>();
        const std::string strategy_str =
            pipe.value()["parameters"].value("strategy", std::string("round_robin"));
        const auto strategy = (strategy_str == "broadcast")
                                  ? DemultiplexerPipe::Strategy::Broadcast
                                  : DemultiplexerPipe::Strategy::RoundRobin;
        created_pipes[pipe_name] =
            std::make_shared<DemultiplexerPipe>(max_candidates, output_slots, strategy);
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

        // Optional cut-off score and full EvolutionParameters set; see
        // `deconstructEvolutionParametersToJson` for the on-disk shape. Both blocks default
        // to the C++-side defaults when missing so older pipeline JSON files keep working.
        if (pipe.value()["parameters"].contains("cut_off_score")) {
          evaluator_pipe->setCutOffScore(
              pipe.value()["parameters"]["cut_off_score"].get<double>());
        }
        if (pipe.value()["parameters"].contains("evolution_parameters")) {
          evaluator_pipe->setEvolutionParameters(constructEvolutionParametersFromJson(
              pipe.value()["parameters"]["evolution_parameters"]));
        }

        pipeline->addPipe(pipe_name, created_pipes[pipe_name]);
      } else {
        // Unknown pipe types used to be silently dropped, which meant the on-disk model
        // could carry a pipe that did not exist in the live `Pipeline` -- connections
        // referencing it would then fail with a confusing "pipe not found" error from the
        // connection loop below. Surface the bad type up-front instead.
        std::string message = "Unknown pipe type '";
        message.append(pipe_type).append("' for pipe '").append(pipe_name).append("'");
        throw std::invalid_argument(message);
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
    } else if (const auto adder_eval =
                   std::dynamic_pointer_cast<AdderEvaluator>(description.evaluator)) {
      evaluator["type"] = "AdderEvaluator";
      evaluator["parameters"]["trial_count"] = adder_eval->getTrialCount();
      evaluator["parameters"]["value_range"] = adder_eval->getValueRange();
      evaluator["parameters"]["max_steps_per_trial"] = adder_eval->getMaxStepsPerTrial();
    } else if (const auto max_eval =
                   std::dynamic_pointer_cast<MaximumEvaluator>(description.evaluator)) {
      evaluator["type"] = "MaximumEvaluator";
      evaluator["parameters"]["input_count"] = max_eval->getInputCount();
      evaluator["parameters"]["trial_count"] = max_eval->getTrialCount();
      evaluator["parameters"]["value_range"] = max_eval->getValueRange();
      evaluator["parameters"]["max_steps_per_trial"] = max_eval->getMaxStepsPerTrial();
    } else if (const auto sha_eval =
                   std::dynamic_pointer_cast<Sha256RoundEvaluator>(description.evaluator)) {
      evaluator["type"] = "Sha256RoundEvaluator";
      evaluator["parameters"]["trial_count"] = sha_eval->getTrialCount();
      evaluator["parameters"]["round_constant_index"] = sha_eval->getRoundConstantIndex();
      evaluator["parameters"]["max_steps_per_trial"] = sha_eval->getMaxStepsPerTrial();
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
      pipe_json["parameters"]["cut_off_score"] = evaluator_pipe->getCutOffScore();
      pipe_json["parameters"]["evolution_parameters"] =
          deconstructEvolutionParametersToJson(evaluator_pipe->getEvolutionParameters());
    } else if (std::dynamic_pointer_cast<EvolutionPipe>(pipe->pipe)) {
      pipe_json["type"] = "EvolutionPipe";
    } else if (std::dynamic_pointer_cast<NullSinkPipe>(pipe->pipe)) {
      pipe_json["type"] = "NullSinkPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
    } else if (auto summary_pipe = std::dynamic_pointer_cast<ResultsSummaryPipe>(pipe->pipe)) {
      pipe_json["type"] = "ResultsSummaryPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["window_size"] = summary_pipe->getWindowSize();
    } else if (auto mux_pipe = std::dynamic_pointer_cast<MultiplexerPipe>(pipe->pipe)) {
      pipe_json["type"] = "MultiplexerPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["input_slots"] = mux_pipe->getInputSlots();
    } else if (auto storage_sink = std::dynamic_pointer_cast<ProgramStorageSinkPipe>(pipe->pipe)) {
      pipe_json["type"] = "ProgramStorageSinkPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["path"] = storage_sink->getPath();
      pipe_json["parameters"]["top_k"] = storage_sink->getTopK();
    } else if (auto storage_source = std::dynamic_pointer_cast<ProgramStorageSourcePipe>(pipe->pipe)) {
      pipe_json["type"] = "ProgramStorageSourcePipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["path"] = storage_source->getPath();
      pipe_json["parameters"]["loop"] = storage_source->getLoop();
    } else if (auto fan_pipe = std::dynamic_pointer_cast<FanPipe>(pipe->pipe)) {
      pipe_json["type"] = "FanPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["window_seconds"] = fan_pipe->getWindowSeconds();
    } else if (auto demux_pipe = std::dynamic_pointer_cast<DemultiplexerPipe>(pipe->pipe)) {
      pipe_json["type"] = "DemultiplexerPipe";
      pipe_json["parameters"]["max_candidates"] = pipe->pipe->getMaxCandidates();
      pipe_json["parameters"]["output_slots"] = demux_pipe->getOutputSlots();
      pipe_json["parameters"]["strategy"] =
          demux_pipe->getStrategy() == DemultiplexerPipe::Strategy::Broadcast
              ? std::string("broadcast")
              : std::string("round_robin");
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
  std::unordered_map<uint32_t, std::deque<Pipeline::PipelineMetrics>> metrics_cache;
  while (should_run_metrics_collector_.load(std::memory_order_acquire)) {
    // Remove the oldest element from the cache for each pipeline if maximum window size is reached.
    for (auto& metrics_pair : metrics_cache) {
      if (metrics_pair.second.size() >= metrics_window_size_) {
        metrics_pair.second.pop_back();
      }
    }

    // Collect the current metrics and push them into the cache.
    {
      std::scoped_lock lock(pipelines_mutex_);
      for (auto& descriptor : pipelines_) {
        metrics_cache[descriptor.id].push_front(descriptor.pipeline->getMetrics());
      }
    }

    // Calculate the current metrics.
    std::unordered_map<uint32_t, Pipeline::PipelineMetrics> metrics;
    for (auto& metrics_pair : metrics_cache) {
      const uint32_t pipeline_id = metrics_pair.first;
      const std::deque<Pipeline::PipelineMetrics>& metrics_history = metrics_pair.second;

      if (metrics_history.empty()) {
        continue;
      }

      const std::chrono::duration<double> elapsed_time =
          std::chrono::system_clock::now() - metrics_history.back().measure_time_start;
      const double duration_seconds = elapsed_time.count();

      Pipeline::PipelineMetrics pipeline_metrics;
      pipeline_metrics.measure_time_start = std::chrono::system_clock::now();

      std::unordered_map<std::string, Pipeline::PipeMetrics>& pipe_counters =
          pipeline_metrics.pipes;

      // Sum up all execution counts, inputs received, and outputs sent from all history items
      for (const auto& pipeline_metrics : metrics_history) {
        for (const auto& pipe_pair : pipeline_metrics.pipes) {
          const std::string& pipe_name = pipe_pair.first;
          const Pipeline::PipeMetrics& current_pipe_metrics = pipe_pair.second;
          Pipeline::PipeMetrics& current_counter = pipe_counters[pipe_name];

          current_counter.execution_count += current_pipe_metrics.execution_count;

          for (const auto& input_pair : current_pipe_metrics.inputs_received) {
            current_counter.inputs_received[input_pair.first] += input_pair.second;
          }
          for (const auto& output_pair : current_pipe_metrics.outputs_sent) {
            current_counter.outputs_sent[output_pair.first] += output_pair.second;
          }
        }
      }

      // Divide by the count of history items and convert to per-second rates
      for (const auto& pipe_pair : pipe_counters) {
        const std::string& pipe_name = pipe_pair.first;
        const Pipeline::PipeMetrics& counters = pipe_pair.second;
        Pipeline::PipeMetrics& averaged_metrics = pipeline_metrics.pipes[pipe_name];

        averaged_metrics.execution_count = counters.execution_count / duration_seconds;

        for (const auto& input_pair : counters.inputs_received) {
          averaged_metrics.inputs_received[input_pair.first] = input_pair.second / duration_seconds;
        }
        for (const auto& output_pair : counters.outputs_sent) {
          averaged_metrics.outputs_sent[output_pair.first] = output_pair.second / duration_seconds;
        }
      }

      // Store the resulting metrics for the pipeline.
      metrics[pipeline_id] = pipeline_metrics;
    }

    // Store the resulting metrics.
    {
      std::scoped_lock lock(metrics_mutex_);
      metrics_ = metrics;
    }

    // Limit cycle time.
    std::chrono::milliseconds interval_time(metrics_interval_time_);
    std::this_thread::sleep_for(interval_time);
  }
}

} // namespace beast
