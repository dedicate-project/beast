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
    const auto& pipes = pipeline.getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "pipe0");
    REQUIRE(std::dynamic_pointer_cast<NullSinkPipe>(pipe->pipe) != nullptr);
  }

  SECTION("NullSinkPipe pipeline is correctly deconstructed to JSON") {
    Pipeline pipeline;
    std::shared_ptr<NullSinkPipe> pipe = std::make_shared<NullSinkPipe>();
    const std::string name = "null_sink_pipe";
    pipeline.addPipe(name, pipe);

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
    const auto& pipes = pipeline.getPipes();

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

  // Clean up temporary files
  std::filesystem::remove_all(temp_storage_path);
}

} // namespace beast
