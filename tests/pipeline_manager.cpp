#include <catch2/catch.hpp>

// Standard
#include <filesystem>

// BEAST
#include <beast/beast.hpp>

namespace beast {

TEST_CASE("PipelineManager") {
  std::filesystem::path temp_storage_path =
      std::filesystem::temp_directory_path() / "test_pipelines";
  PipelineManager manager(temp_storage_path.string());

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

    PipelineManager manager_2(temp_storage_path.string());
    const std::list<PipelineManager::PipelineDescriptor>& pipelines = manager.getPipelines();

    REQUIRE(pipelines.size() == 1);
    REQUIRE(pipelines.front().name == pipeline_name);
    REQUIRE(pipelines.front().filename == "Some_test_pipeline.json");
  }

  SECTION("NullSinkPipe pipeline is correctly constructed from JSON") {
    const auto json = R"({"pipes":{"pipe0":{"type":"NullSinkPipe"}}})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "pipe0");
    REQUIRE(std::dynamic_pointer_cast<NullSinkPipe>(pipe->pipe) != nullptr);
  }

  SECTION("NullSinkPipe pipeline is correctly deconstructed to JSON") {
    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    std::shared_ptr<NullSinkPipe> pipe = std::make_shared<NullSinkPipe>();
    const std::string name = "null_sink_pipe";
    pipeline->addPipe(name, pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);

    REQUIRE(json.contains("pipes") == true);
    REQUIRE(json["pipes"].size() == 1);
    REQUIRE(json["pipes"].contains(name) == true);
    REQUIRE(json["pipes"][name]["type"].get<std::string>() == "NullSinkPipe");
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

  // Clean up temporary files
  std::filesystem::remove_all(temp_storage_path);
}

} // namespace beast
