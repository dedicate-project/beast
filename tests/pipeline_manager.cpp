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

  // Clean up temporary files
  std::filesystem::remove_all(temp_storage_path);
}

} // namespace beast
