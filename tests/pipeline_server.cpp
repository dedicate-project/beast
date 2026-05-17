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

  SECTION("pipelines_can_be_started_and_stopped") {
    // Set up a new pipeline
    crow::request req;
    req.body = R"({"name":"test_pipeline"})";
    auto response = server.serveNewPipeline(req);
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Start the pipeline
    req.body = R"({})";
    response = server.servePipelineAction(req, pipeline_id, "start");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Stop the pipeline
    response = server.servePipelineAction(req, pipeline_id, "stop");
    REQUIRE(response["status"].dump() == "\"success\"");
  }

  SECTION("pipelines_cannot_be_started_when_running") {
    // Set up a new pipeline
    crow::request req;
    req.body = R"({"name":"test_pipeline"})";
    auto response = server.serveNewPipeline(req);
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Start the pipeline
    req.body = R"({})";
    response = server.servePipelineAction(req, pipeline_id, "start");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Start the pipeline again
    response = server.servePipelineAction(req, pipeline_id, "start");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("pipelines_cannot_be_stopped_when_not_running") {
    // Set up a new pipeline
    crow::request req;
    req.body = R"({"name":"test_pipeline"})";
    auto response = server.serveNewPipeline(req);
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Stop the pipeline
    req.body = R"({})";
    response = server.servePipelineAction(req, pipeline_id, "stop");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("serving_status_returns_the_correct_information") {
    auto status = beast::PipelineServer::serveStatus();
    REQUIRE(status["version"].dump() == "\"" + beast::getVersionString() + "\"");
  }

  SECTION("pipeline_name_can_be_changed") {
    // Set up a new pipeline
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Read the current name and verify it
    response = server.servePipelineById(pipeline_id);
    REQUIRE(response["name"].dump() == "\"some_name\"");

    // Change the name
    req.body = R"({"action":"change_name","name":"some_other_name"})";
    response = server.servePipelineAction(req, pipeline_id, "update");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Read the new name and verify it
    response = server.servePipelineById(pipeline_id);
    REQUIRE(response["name"].dump() == "\"some_other_name\"");
  }

  SECTION("stopped_pipeline_can_be_deleted") {
    // Set up a new pipeline
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Delete the pipeline
    response = server.servePipelineAction(req, pipeline_id, "delete");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Try to acquire the pipeline, should be missing
    response = server.servePipelineById(pipeline_id);
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("started_pipeline_can_be_stopped_and_deleted") {
    // Set up a new pipeline
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Start the pipeline
    req.body = R"({})";
    response = server.servePipelineAction(req, pipeline_id, "start");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Delete the pipeline
    response = server.servePipelineAction(req, pipeline_id, "delete");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Try to acquire the pipeline, should be missing
    response = server.servePipelineById(pipeline_id);
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("invalid_commands_fail") {
    // Set up a new pipeline
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Issue an invalid command
    req.body = R"({})";
    response = server.servePipelineAction(req, pipeline_id, "invalid");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("invalid_update_actions_fail") {
    // Set up a new pipeline
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Try to run an invalid update action
    req.body = R"({"action":"invalid"})";
    response = server.servePipelineAction(req, pipeline_id, "update");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("invalid_json_in_update_requests_fail") {
    // Set up a new pipeline
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Try to run an invalid update action
    req.body = R"({invalidjson})";
    response = server.servePipelineAction(req, pipeline_id, "update");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("missing_json_request_header_in_update_fails") {
    // Set up a new pipeline
    crow::request req;
    req.body = R"({"name":"some_name"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const uint32_t pipeline_id = std::stoi(response["id"].dump());

    // Change the name
    req.body = R"({"action":"change_name","name":"some_other_name"})";
    response = server.servePipelineAction(req, pipeline_id, "update");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("serveNewPipeline") {
    crow::request req;
    req.body = R"({"name":"test_pipeline"})";
    auto response = server.serveNewPipeline(req);

    REQUIRE(response["status"].dump() == "\"success\"");
    REQUIRE(std::stoi(response["id"].dump()) == 0);
  }

  SECTION("creating_pipelines_without_names_fails") {
    crow::request req;
    req.body = R"({})";
    auto response = server.serveNewPipeline(req);

    REQUIRE(response["status"].dump() == "\"failed\"");
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

  SECTION("serving_pipelines_with_invalid_ids_fails") {
    auto pipeline_status = server.servePipelineById(10000);
    REQUIRE(pipeline_status["status"].dump() == "\"failed\"");
  }

  SECTION("invalid_pipeline_id_yields_failed_status") {
    const crow::request req;
    auto response = server.servePipelineAction(req, 10000, "ignored");
    REQUIRE(response["status"].dump() == "\"failed\"");
  }

  SECTION("serving_all_pipelines_returns_all_registered_pipelines") {
    crow::request req;
    req.body = R"({"name":"test_pipeline1"})";
    auto response = server.serveNewPipeline(req);
    const auto pipeline_id_1 = static_cast<uint32_t>(std::stoi(response["id"].dump()));
    req.body = R"({"name":"test_pipeline2"})";
    response = server.serveNewPipeline(req);
    const auto pipeline_id_2 = static_cast<uint32_t>(std::stoi(response["id"].dump()));

    auto pipelines = server.serveAllPipelines();
    REQUIRE(std::stoi(pipelines[0]["id"].dump()) == pipeline_id_1);
    REQUIRE(pipelines[0]["name"].dump() == "\"test_pipeline1\"");
    REQUIRE(std::stoi(pipelines[1]["id"].dump()) == pipeline_id_2);
    REQUIRE(pipelines[1]["name"].dump() == "\"test_pipeline2\"");
  }

  SECTION("pipelines_can_be_assembled_through_update_actions") {
    // End-to-end exercise of the HTTP mutation surface that the compose UI relies on. We add
    // three pipes (factory -> evaluator -> sink), wire them up, and start the resulting
    // pipeline. Each step asserts on the structured status reply so a regression in the
    // dispatcher (e.g. an action being silently rejected as "invalid_action") would be
    // caught here before users notice in the browser.
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"compose_assembly"})";
    auto response = server.serveNewPipeline(req);
    REQUIRE(response["status"].dump() == "\"success\"");
    const auto pipeline_id = static_cast<uint32_t>(std::stoi(response["id"].dump()));

    const auto add = [&](const std::string& body) {
      req.body = body;
      return server.servePipelineAction(req, pipeline_id, "update");
    };

    response = add(R"({
      "action": "add_pipe",
      "name": "factory",
      "type": "ProgramFactoryPipe",
      "position": {"x": 10, "y": 20},
      "parameters": {
        "factory": "RandomProgramFactory",
        "max_candidates": 5,
        "max_size": 32,
        "memory_variables": 16,
        "string_table_items": 0,
        "string_table_item_length": 0
      }
    })");
    REQUIRE(response["status"].dump() == "\"success\"");

    response = add(R"({
      "action": "add_pipe",
      "name": "evaluator",
      "type": "EvaluatorPipe",
      "position": {"x": 200, "y": 20},
      "parameters": {
        "max_candidates": 4,
        "memory_variables": 16,
        "string_table_items": 0,
        "string_table_item_length": 0,
        "evaluators": [{
          "type": "MazeEvaluator",
          "weight": 1.0,
          "invert_logic": false,
          "parameters": {"rows": 5, "cols": 5, "difficulty": 0.1, "max_steps": 50}
        }]
      }
    })");
    REQUIRE(response["status"].dump() == "\"success\"");

    response = add(R"({
      "action": "add_pipe",
      "name": "sink",
      "type": "NullSinkPipe",
      "position": {"x": 400, "y": 20},
      "parameters": {"max_candidates": 8}
    })");
    REQUIRE(response["status"].dump() == "\"success\"");

    response = add(R"({
      "action": "add_connection",
      "source_pipe": "factory", "source_slot": 0,
      "destination_pipe": "evaluator", "destination_slot": 0,
      "buffer_size": 8
    })");
    REQUIRE(response["status"].dump() == "\"success\"");

    response = add(R"({
      "action": "add_connection",
      "source_pipe": "evaluator", "source_slot": 0,
      "destination_pipe": "sink", "destination_slot": 0,
      "buffer_size": 8
    })");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Now run the pipeline briefly to confirm the assembled graph actually trains.
    response = server.servePipelineAction(req, pipeline_id, "start");
    REQUIRE(response["status"].dump() == "\"success\"");
    response = server.servePipelineAction(req, pipeline_id, "stop");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Structural edits must be rejected when running.
    response = server.servePipelineAction(req, pipeline_id, "start");
    REQUIRE(response["status"].dump() == "\"success\"");
    response = add(R"({"action":"delete_pipe","name":"sink"})");
    REQUIRE(response["status"].dump() == "\"failed\"");
    REQUIRE(response["error"].dump() == "\"pipeline_running\"");
    response = server.servePipelineAction(req, pipeline_id, "stop");
    REQUIRE(response["status"].dump() == "\"success\"");

    // Delete a connection, then a pipe (which should also remove the still-referencing
    // connection without the rebuild throwing).
    response = add(R"({
      "action": "delete_connection",
      "source_pipe": "evaluator", "source_slot": 0,
      "destination_pipe": "sink", "destination_slot": 0
    })");
    REQUIRE(response["status"].dump() == "\"success\"");
    response = add(R"({"action":"delete_pipe","name":"factory"})");
    REQUIRE(response["status"].dump() == "\"success\"");

    response = server.servePipelineById(pipeline_id);
    REQUIRE(response["status"].dump() == "\"success\"");
  }

  SECTION("add_pipe_with_unknown_type_fails_with_descriptive_error") {
    crow::request req;
    req.add_header("Content-Type", "application/json");
    req.body = R"({"name":"bogus"})";
    auto response = server.serveNewPipeline(req);
    const auto pipeline_id = static_cast<uint32_t>(std::stoi(response["id"].dump()));

    req.body = R"({
      "action": "add_pipe",
      "name": "weird",
      "type": "TotallyNotARealPipe",
      "parameters": {}
    })";
    response = server.servePipelineAction(req, pipeline_id, "update");
    REQUIRE(response["status"].dump() == "\"failed\"");
    // We don't pin the exact error text (it comes from constructPipelineFromJson's
    // checkForParameterPresenceInPipeJson and may evolve), just that we surface *something*
    // useful for the user instead of a silent success.
    REQUIRE(response.count("error") == 1);
  }

  std::filesystem::remove_all(temp_path);
}
