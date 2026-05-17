#include <catch2/catch.hpp>

// Standard
#include <filesystem>

// BEAST
#include <beast/beast.hpp>

namespace beast {

TEST_CASE("PipelineManager") {
  std::filesystem::path temp_storage_path =
      std::filesystem::temp_directory_path() / "test_pipelines";
  PipelineManager manager(temp_storage_path.string(), 100, 50);

  SECTION("Create and get pipeline") {
    const std::string pipeline_name = "Test pipeline";
    const uint32_t id = manager.createPipeline(pipeline_name);
    const PipelineManager::PipelineDescriptor& pipeline = manager.getPipelineById(id);

    REQUIRE(pipeline.id == id);
    REQUIRE(pipeline.name == pipeline_name);
    REQUIRE(pipeline.filename == "Test_pipeline.json");
  }

  SECTION("Get non-existent pipeline") {
    REQUIRE_THROWS_AS(manager.getPipelineById(100), std::invalid_argument);
  }

  SECTION("Get all pipelines") {
    const uint32_t id1 = manager.createPipeline("Test Pipeline 1");
    const uint32_t id2 = manager.createPipeline("Test Pipeline 2");

    const std::list<PipelineManager::PipelineDescriptor>& pipelines = manager.getPipelines();

    REQUIRE(pipelines.size() == 2);
    REQUIRE(pipelines.front().id == id1);
    REQUIRE(pipelines.back().id == id2);
  }

  SECTION("Load pipelines from disk") {
    const std::string pipeline_name = "Some test pipeline";
    const uint32_t id = manager.createPipeline(pipeline_name);
    static_cast<void>(id);

    PipelineManager manager_2(temp_storage_path.string(), 100, 50);
    const std::list<PipelineManager::PipelineDescriptor>& pipelines = manager.getPipelines();

    REQUIRE(pipelines.size() == 1);
    REQUIRE(pipelines.front().name == pipeline_name);
    REQUIRE(pipelines.front().filename == "Some_test_pipeline.json");
  }

  SECTION("NullSinkPipe pipeline is correctly constructed from JSON") {
    const auto json =
        R"({"pipes":{"pipe0":{"type":"NullSinkPipe","parameters":{"max_candidates":10}}}})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "pipe0");
    REQUIRE(std::dynamic_pointer_cast<NullSinkPipe>(pipe->pipe) != nullptr);
  }

  SECTION("NullSinkPipe pipeline is correctly deconstructed to JSON") {
    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    std::shared_ptr<NullSinkPipe> pipe = std::make_shared<NullSinkPipe>(10);
    const std::string name = "null_sink_pipe";
    pipeline->addPipe(name, pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);

    REQUIRE(json.contains("pipes") == true);
    REQUIRE(json["pipes"].size() == 1);
    REQUIRE(json["pipes"].contains(name) == true);
    REQUIRE(json["pipes"][name]["type"].get<std::string>() == "NullSinkPipe");
  }

  SECTION("ResultsSummaryPipe round-trips through JSON serialisation") {
    const auto json = R"({
        "pipes": {
          "summary": {
            "type": "ResultsSummaryPipe",
            "parameters": { "max_candidates": 16, "window_size": 64 }
          }
        }})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();
    REQUIRE(pipes.size() == 1);
    const auto summary_pipe =
        std::dynamic_pointer_cast<ResultsSummaryPipe>(pipes.front()->pipe);
    REQUIRE(summary_pipe != nullptr);
    REQUIRE(summary_pipe->getMaxCandidates() == 16);
    REQUIRE(summary_pipe->getWindowSize() == 64);

    // Round-tripping back to JSON preserves the parameters so the persisted on-disk
    // representation is stable across save/load cycles.
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["summary"]["type"].get<std::string>() == "ResultsSummaryPipe");
    REQUIRE(back["pipes"]["summary"]["parameters"]["max_candidates"].get<uint32_t>() == 16);
    REQUIRE(back["pipes"]["summary"]["parameters"]["window_size"].get<uint32_t>() == 64);
  }

  SECTION("ResultsSummaryPipe defaults window_size when omitted") {
    // Omitting `window_size` should pick the built-in default (256) rather than throwing
    // -- the UI exposes the knob as optional and persists 0 when the user leaves it
    // blank.
    const auto json = R"({
        "pipes": {
          "summary": {
            "type": "ResultsSummaryPipe",
            "parameters": { "max_candidates": 4 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto summary_pipe =
        std::dynamic_pointer_cast<ResultsSummaryPipe>(pipeline->getPipes().front()->pipe);
    REQUIRE(summary_pipe != nullptr);
    REQUIRE(summary_pipe->getWindowSize() == 256);
  }

  SECTION("EvaluatorPipe+MazeEvaluator pipeline is correctly constructed from JSON") {
    const auto json = R"({
        "pipes": {
          "eval_pipe": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 10,
              "memory_variables": 5,
              "string_table_items": 2,
              "string_table_item_length": 25,
              "evaluators": [
                {
                  "type": "MazeEvaluator",
                  "parameters": {
                    "rows": 10,
                    "cols": 12,
                    "difficulty": 0.61,
                    "max_steps": 1325
                  },
                  "weight": 1.0,
                  "invert_logic": false
                }
              ]
            }
          }
        }})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "eval_pipe");
    const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(pipe->pipe);
    REQUIRE(eval_pipe != nullptr);
    const auto& evaluators = eval_pipe->getEvaluators();
    REQUIRE(evaluators.size() == 1);
    const auto& maze_eval_desc = evaluators.front();
    REQUIRE(maze_eval_desc.weight == 1.0);
    REQUIRE(maze_eval_desc.invert_logic == false);
    const auto maze_eval = std::dynamic_pointer_cast<MazeEvaluator>(maze_eval_desc.evaluator);
    REQUIRE(maze_eval != nullptr);
    REQUIRE(maze_eval->getRows() == 10);
    REQUIRE(maze_eval->getCols() == 12);
    REQUIRE(maze_eval->getDifficulty() == 0.61);
    REQUIRE(maze_eval->getMaxSteps() == 1325);
  }

  SECTION("EvaluatorPipe+MazeEvaluator pipeline is correctly deconstructed to JSON") {
    // Evaluator pipe parameters
    const uint32_t max_candidates = 128;
    const uint32_t memory_size = 12;
    const uint32_t string_table_items = 3;
    const uint32_t string_table_item_length = 25;
    const std::string name = "maze_eval_pipe";

    // Maze evaluator parameters
    const uint32_t rows = 99;
    const uint32_t cols = 51;
    const double difficulty = 0.73;
    const uint32_t max_steps = 1024;

    const double weight = 0.57;
    const bool invert_logic = false;

    std::shared_ptr<EvaluatorPipe> pipe = std::make_shared<EvaluatorPipe>(
        max_candidates, memory_size, string_table_items, string_table_item_length);
    std::shared_ptr<Evaluator> eval =
        std::make_shared<MazeEvaluator>(rows, cols, difficulty, max_steps);
    pipe->addEvaluator(eval, weight, invert_logic);

    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    pipeline->addPipe(name, pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);

    REQUIRE(json.contains("pipes") == true);
    REQUIRE(json["pipes"].size() == 1);
    REQUIRE(json["pipes"].contains(name) == true);
    REQUIRE(json["pipes"][name]["type"].get<std::string>() == "EvaluatorPipe");
    REQUIRE(json["pipes"][name]["parameters"]["max_candidates"] == max_candidates);
    REQUIRE(json["pipes"][name]["parameters"]["memory_variables"] == memory_size);
    REQUIRE(json["pipes"][name]["parameters"]["string_table_items"] == string_table_items);
    REQUIRE(json["pipes"][name]["parameters"]["string_table_item_length"] ==
            string_table_item_length);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"].size() == 1);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["type"] == "MazeEvaluator");
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["weight"] == weight);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["invert_logic"] == invert_logic);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["max_steps"] ==
            max_steps);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["difficulty"] ==
            difficulty);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["rows"] == rows);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["cols"] == cols);
  }

  SECTION("EvaluatorPipe+MazeEvaluator pipeline reconstruction through JSON works") {
    // Evaluator pipe parameters
    const uint32_t max_candidates = 128;
    const uint32_t memory_size = 12;
    const uint32_t string_table_items = 3;
    const uint32_t string_table_item_length = 25;
    const std::string name = "maze_eval_pipe";

    // Maze evaluator parameters
    const uint32_t rows = 99;
    const uint32_t cols = 51;
    const double difficulty = 0.73;
    const uint32_t max_steps = 1024;

    const double weight = 0.57;
    const bool invert_logic = false;

    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    {
      std::shared_ptr<EvaluatorPipe> pipe = std::make_shared<EvaluatorPipe>(
          max_candidates, memory_size, string_table_items, string_table_item_length);
      std::shared_ptr<Evaluator> eval =
          std::make_shared<MazeEvaluator>(rows, cols, difficulty, max_steps);
      pipe->addEvaluator(eval, weight, invert_logic);
      pipeline->addPipe(name, pipe);
    }

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);
    const auto reconstructed_pipeline = PipelineManager::constructPipelineFromJson(json);

    {
      const auto& managed_pipes = reconstructed_pipeline->getPipes();
      REQUIRE(managed_pipes.size() == 1);
      const auto& managed_pipe = managed_pipes.front();
      REQUIRE(managed_pipe->name == name);
      const auto pipe = std::dynamic_pointer_cast<EvaluatorPipe>(managed_pipe->pipe);
      REQUIRE(pipe->getMemorySize() == memory_size);
      REQUIRE(pipe->getMaxCandidates() == max_candidates);
      REQUIRE(pipe->getStringTableSize() == string_table_items);
      REQUIRE(pipe->getStringTableItemLength() == string_table_item_length);
      REQUIRE(pipe->getStringTableSize() == string_table_items);

      const auto& evaluator_descriptions = pipe->getEvaluators();
      REQUIRE(evaluator_descriptions.size() == 1);
      const auto& evaluator_description = evaluator_descriptions.front();
      REQUIRE(evaluator_description.weight == weight);
      REQUIRE(evaluator_description.invert_logic == invert_logic);
      const auto evaluator =
          std::dynamic_pointer_cast<MazeEvaluator>(evaluator_description.evaluator);
      REQUIRE(evaluator != nullptr);
      REQUIRE(evaluator->getRows() == rows);
      REQUIRE(evaluator->getCols() == cols);
      REQUIRE(evaluator->getDifficulty() == difficulty);
      REQUIRE(evaluator->getMaxSteps() == max_steps);
    }
  }

  SECTION("EvaluatorPipe round-trips EvolutionParameters and cut_off_score through JSON") {
    // Regression test for the previously-empty EvolutionPipe JSON branch: ensure both the
    // GA knobs and the cut-off score survive a deconstruct/reconstruct cycle, including the
    // OpcodeWeights map.
    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    auto pipe = std::make_shared<EvaluatorPipe>(/*max_candidates=*/16,
                                                /*memory_variables=*/8,
                                                /*string_table_items=*/2,
                                                /*string_table_item_length=*/4);
    pipe->addEvaluator(std::make_shared<MazeEvaluator>(5, 5, 0.2, 100), 1.0, false);
    pipe->setCutOffScore(0.42);

    EvolutionPipe::EvolutionParameters params;
    params.generations = 7;
    params.crossover_probability = 0.6;
    params.mutation_probability = 0.15;
    params.elitism = false;
    params.byte_mutation_share = 0.33;
    params.variable_count = 32;
    params.string_table_size = 3;
    params.string_table_item_length = 7;
    params.max_genome_bytes = 512;
    params.starting_program_size = 48;
    params.opcode_weights[OpCode::NoOp] = 0.0;
    params.opcode_weights[OpCode::SetVariable] = 4.5;
    params.opcode_weights[OpCode::Terminate] = 0.1;
    pipe->setEvolutionParameters(params);

    pipeline->addPipe("eval", pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);
    const auto rebuilt = PipelineManager::constructPipelineFromJson(json);
    const auto rebuilt_pipe =
        std::dynamic_pointer_cast<EvaluatorPipe>(rebuilt->getPipes().front()->pipe);
    REQUIRE(rebuilt_pipe != nullptr);
    REQUIRE(rebuilt_pipe->getCutOffScore() == 0.42);

    const auto& rebuilt_params = rebuilt_pipe->getEvolutionParameters();
    REQUIRE(rebuilt_params.generations == params.generations);
    REQUIRE(rebuilt_params.crossover_probability == params.crossover_probability);
    REQUIRE(rebuilt_params.mutation_probability == params.mutation_probability);
    REQUIRE(rebuilt_params.elitism == params.elitism);
    REQUIRE(rebuilt_params.byte_mutation_share == params.byte_mutation_share);
    REQUIRE(rebuilt_params.variable_count == params.variable_count);
    REQUIRE(rebuilt_params.string_table_size == params.string_table_size);
    REQUIRE(rebuilt_params.string_table_item_length == params.string_table_item_length);
    REQUIRE(rebuilt_params.max_genome_bytes == params.max_genome_bytes);
    REQUIRE(rebuilt_params.starting_program_size == params.starting_program_size);
    REQUIRE(rebuilt_params.opcode_weights.size() == params.opcode_weights.size());
    for (const auto& [opcode, weight] : params.opcode_weights) {
      REQUIRE(rebuilt_params.opcode_weights.count(opcode) == 1);
      REQUIRE(rebuilt_params.opcode_weights.at(opcode) == weight);
    }
  }

  SECTION("EvaluatorPipe with invalid evaluator pipeline constructed from JSON throw") {
    const auto json = R"({
        "pipes": {
          "eval_pipe": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 10,
              "memory_variables": 5,
              "string_table_items": 2,
              "string_table_item_length": 25,
              "evaluators": [
                {
                  "type": "Some invalid evaluator",
                  "weight": 1.0,
                  "invert_logic": false
                }
              ]
            }
          }
        }})"_json;

    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json), std::invalid_argument);
  }

  SECTION("Pipe without type in pipeline constructed from JSON throw") {
    const auto json = R"({
        "pipes": {
          "eval_pipe": {
          }
        }})"_json;

    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json), std::invalid_argument);
  }

  SECTION("ProgramFactoryPipe pipeline is correctly constructed from JSON") {
    const auto json = R"({
        "pipes": {
          "factory_pipe": {
            "type": "ProgramFactoryPipe",
            "parameters": {
              "max_candidates": 11,
              "max_size": 100,
              "memory_variables": 3,
              "string_table_items": 20,
              "string_table_item_length": 71,
              "factory": "RandomProgramFactory"
            }
          }
        }})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "factory_pipe");
    const auto factory_pipe = std::dynamic_pointer_cast<ProgramFactoryPipe>(pipe->pipe);
    REQUIRE(factory_pipe != nullptr);
    REQUIRE(factory_pipe->getMaxCandidates() == 11);
    REQUIRE(factory_pipe->getMaxSize() == 100);
    REQUIRE(factory_pipe->getMemorySize() == 3);
    REQUIRE(factory_pipe->getStringTableSize() == 20);
    REQUIRE(factory_pipe->getStringTableItemLength() == 71);
    const auto& factory =
        std::dynamic_pointer_cast<RandomProgramFactory>(factory_pipe->getFactory());
    REQUIRE(factory != nullptr);
  }

  SECTION("ProgramFactoryPipe pipeline with invalid factory type from JSON throws") {
    const auto json = R"({
        "pipes": {
          "factory_pipe": {
            "type": "ProgramFactoryPipe",
            "parameters": {
              "max_candidates": 11,
              "max_size": 100,
              "memory_variables": 3,
              "string_table_items": 20,
              "string_table_item_length": 71,
              "factory": "Some invalid factory type"
            }
          }
        }})"_json;

    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json), std::invalid_argument);
  }

  SECTION("mutatePipeline adds, connects, and removes pipes via JSON edits") {
    // Drives the same JSON shape the HTTP layer's add_pipe / add_connection / delete_pipe
    // / delete_connection handlers produce, end-to-end through the manager. Validates that
    // each edit reaches the live Pipeline object (not just the on-disk model).
    const uint32_t id = manager.createPipeline("ui-test");

    // 1. Add a ProgramFactoryPipe.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& metadata) {
      model["pipes"]["factory"] = R"({
        "type": "ProgramFactoryPipe",
        "parameters": {
          "factory": "RandomProgramFactory",
          "max_candidates": 5,
          "max_size": 32,
          "memory_variables": 16,
          "string_table_items": 0,
          "string_table_item_length": 0
        }
      })"_json;
      metadata["pipes"]["factory"]["position"] = {{"x", 10}, {"y", 20}};
    });
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == 1);
    REQUIRE(manager.getPipelineById(id).metadata["pipes"]["factory"]["position"]["x"] == 10);

    // 2. Add a NullSinkPipe and connect them.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["sink"] = R"({
        "type": "NullSinkPipe",
        "parameters": { "max_candidates": 10 }
      })"_json;
      if (!model.contains("connections") || !model["connections"].is_array()) {
        model["connections"] = nlohmann::json::array();
      }
      model["connections"].push_back(R"({
        "source_pipe": "factory",
        "source_slot": 0,
        "destination_pipe": "sink",
        "destination_slot": 0,
        "buffer_size": 4
      })"_json);
    });
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == 2);
    REQUIRE(manager.getPipelineById(id).pipeline->getConnections().size() == 1);

    // 3. Delete the factory; the connection referencing it must come along to satisfy
    //    constructPipelineFromJson, otherwise rebuild would throw.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& metadata) {
      model["pipes"].erase("factory");
      auto& connections = model["connections"];
      connections.erase(std::remove_if(connections.begin(), connections.end(),
                                       [](const nlohmann::json& c) {
                                         return c["source_pipe"] == "factory" ||
                                                c["destination_pipe"] == "factory";
                                       }),
                        connections.end());
      metadata["pipes"].erase("factory");
    });
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == 1);
    REQUIRE(manager.getPipelineById(id).pipeline->getConnections().empty());
  }

  SECTION("mutatePipeline refuses to mutate a running pipeline") {
    const uint32_t id = manager.createPipeline("running-test");
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["sink"] = R"({
        "type": "NullSinkPipe",
        "parameters": { "max_candidates": 4 }
      })"_json;
    });
    manager.getPipelineById(id).pipeline->start();
    REQUIRE_THROWS_AS(manager.mutatePipeline(id,
                                             [](nlohmann::json& model,
                                                nlohmann::json& /*metadata*/) {
                                               model["pipes"]["sink2"] = R"({
            "type": "NullSinkPipe", "parameters": { "max_candidates": 1 }
          })"_json;
                                             }),
                      std::invalid_argument);
    manager.getPipelineById(id).pipeline->stop();
  }

  SECTION("mutatePipeline rolls back when validation fails") {
    // Regression test: a malformed edit (here, declaring a connection to a non-existent
    // pipe) must leave the live Pipeline and on-disk model unchanged because
    // constructPipelineFromJson throws during the validation step.
    const uint32_t id = manager.createPipeline("rollback-test");
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["sink"] = R"({
        "type": "NullSinkPipe",
        "parameters": { "max_candidates": 1 }
      })"_json;
    });
    const auto pipes_before = manager.getPipelineById(id).pipeline->getPipes().size();
    REQUIRE_THROWS_AS(manager.mutatePipeline(id,
                                             [](nlohmann::json& model,
                                                nlohmann::json& /*metadata*/) {
                                               model["connections"].push_back(R"({
            "source_pipe": "ghost",
            "source_slot": 0,
            "destination_pipe": "sink",
            "destination_slot": 0,
            "buffer_size": 1
          })"_json);
                                             }),
                      std::invalid_argument);
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == pipes_before);
  }

  // Clean up temporary files
  std::filesystem::remove_all(temp_storage_path);
}

} // namespace beast
