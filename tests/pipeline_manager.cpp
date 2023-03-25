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
    const uint32_t id = manager.createPipeline("Test Pipeline");
    const PipelineManager::PipelineDescriptor& pipeline = manager.getPipelineById(id);
    REQUIRE(pipeline.id == id);
    REQUIRE(pipeline.name == "Test Pipeline");
    REQUIRE(pipeline.filename == "Test_Pipeline.json");
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

  // Clean up temporary files
  std::filesystem::remove_all(temp_storage_path);
}

}  // namespace beast
