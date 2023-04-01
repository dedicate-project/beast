#include <catch2/catch.hpp>

// Standard
#include <filesystem>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("PipelineServer") {
  const auto temp_path = std::filesystem::temp_directory_path() / "beast_test";
  if (!std::filesystem::exists(temp_path)) {
    std::filesystem::create_directories(temp_path);
  }

  beast::PipelineServer server(temp_path.string());

  SECTION("serveStatus") {
    auto status = beast::PipelineServer::serveStatus();
    REQUIRE(status["version"].dump() == "\"" + beast::getVersionString() + "\"");
  }

  SECTION("serveNewPipeline") {
    crow::request req;
    req.body = R"({"name":"test_pipeline"})";
    auto response = server.serveNewPipeline(req);

    REQUIRE(response["status"].dump() == "\"success\"");
    REQUIRE(std::stoi(response["id"].dump()) == 0);
  }

  SECTION("servePipelineById") {
    crow::request req;
    req.body = R"({"name":"test_pipeline"})";
    auto response = server.serveNewPipeline(req);
    const auto pipeline_id = static_cast<uint32_t>(std::stoi(response["id"].dump()));

    auto pipeline_status = server.servePipelineById(pipeline_id);
    REQUIRE(std::stoi(pipeline_status["id"].dump()) == pipeline_id);
    REQUIRE(pipeline_status["state"].dump() == "\"stopped\"");
  }

  std::filesystem::remove_all(temp_path);
}
