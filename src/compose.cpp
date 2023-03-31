// Standard
#include <cstdlib>
#include <iostream>
#include <list>
#include <string>

// BEAST
#include <beast/beast.hpp>

// Crow
#define CROW_DISABLE_STATIC_DIR
#include <crow.h>

// CLI11
#include <CLI/CLI.hpp>

/**
 * Serve a JSON response containing the version of the application.
 * 
 * @return JSON response containing version information.
 */
crow::json::wvalue serveStatus() {
  crow::json::wvalue value;
  const auto version = beast::getVersion();
  value["version"] =
      std::to_string(version[0]) + "." +
      std::to_string(version[1]) + "." +
      std::to_string(version[2]);
  return value;
}

/**
 * Serve a JSON response for creating a new pipeline.
 * 
 * @param req Request object.
 * @param pipeline_manager Pointer to the PipelineManager instance.
 * @return JSON response containing pipeline creation status.
 */
crow::json::wvalue serveNewPipeline(
    const crow::request& req, beast::PipelineManager* pipeline_manager) {
  crow::json::wvalue value;
  const auto req_body = crow::json::load(req.body);
  if (req_body && req_body.has("name")) {
    const std::string name = static_cast<std::string>(req_body["name"]);
    try {
      const int32_t id = pipeline_manager->createPipeline(name);
      value["status"] = "success";
      value["id"] = id;
    } catch (const std::runtime_error& exception) {
      value["status"] = "failed";
      value["error"] = exception.what();
    }
  } else {
    value["status"] = "failed";
    value["error"] = "Missing 'name' in request body";
  }
  return value;
}

/**
 * Serve a JSON response for getting pipeline status by ID.
 * 
 * @param pipeline_manager Pointer to the PipelineManager instance.
 * @param pipeline_id ID of the pipeline.
 * @return JSON response containing pipeline status.
 */
crow::json::wvalue servePipelineById(
    beast::PipelineManager* pipeline_manager, int32_t pipeline_id) {
  crow::json::wvalue value;
  value["id"] = pipeline_id;
  try {
    const auto& descriptor = pipeline_manager->getPipelineById(pipeline_id);
    value["state"] =
        descriptor.pipeline.isRunning() ? std::string("running") : std::string("stopped");
  } catch (const std::runtime_error& exception) {
    value["status"] = "failed";
    value["error"] = exception.what();
  }
  return value;
}

/**
 * Serve a JSON response for handling pipeline actions.
 * 
 * Supported JSON request actions:
 *  - "start": start the pipeline. Returns "already_running" error if pipeline is already running.
 *  - "stop": stop the pipeline. Returns "not_running" error if pipeline is not running.
 *  - "update": update the pipeline. Expects a JSON payload with "action" and "name" fields.
 *    Supported update actions:
 *     - "change_name": changes the name of the pipeline.
 *  - "delete": delete the pipeline.
 *
 * @param req Request object.
 * @param pipeline_manager Pointer to the PipelineManager instance.
 * @param pipeline_id ID of the pipeline.
 * @param path Path of the action.
 * @return JSON response containing pipeline action status.
 */
crow::json::wvalue servePipelineAction(
    const crow::request& req, beast::PipelineManager* pipeline_manager, int32_t pipeline_id,
    const std::string_view path) {
  crow::json::wvalue value;
  value["id"] = pipeline_id;
  try {
    auto& pipeline = pipeline_manager->getPipelineById(pipeline_id);
    const bool running = pipeline.pipeline.isRunning();
    if (path == "start") {
      if (running) {
        value["status"] = "failed";
        value["error"] = "already_running";
      } else {
        try {
          pipeline.pipeline.start();
          value["status"] = "success";
        } catch (const std::runtime_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      }
    } else if (path == "stop") {
      if (running) {
        try {
          pipeline.pipeline.stop();
          value["status"] = "success";
        } catch (const std::runtime_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      } else {
        value["status"] = "failed";
        value["error"] = "not_running";
      }
    } else if (path == "update") {
      if (req.get_header_value("Content-Type") == "application/json") {
        try {
          const auto req_body = crow::json::load(req.body);
          const auto action = static_cast<std::string>(req_body["action"]);

          if (action == "change_name") {
            const auto new_name = static_cast<std::string>(req_body["name"]);
            pipeline_manager->updatePipelineName(pipeline.id, new_name);
            value["status"] = "success";
          } else {
            value["status"] = "failed";
            value["action"] = action;
            value["error"] = "invalid_action";
          }
        } catch (const std::runtime_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      } else {
        value["status"] = "failed";
        value["error"] = "invalid_request";
      }
    } else if (path == "delete") {
      pipeline.pipeline.stop();
      pipeline_manager->deletePipeline(pipeline.id);
      value["status"] = "success";
    } else {
      value["status"] = "failed";
      value["error"] = "invalid_command";
      value["command"] = std::string(path);
    }
  } catch (const std::runtime_error& exception) {
    value["status"] = "failed";
    value["error"] = exception.what();
  }
  return value;
}

/**
 * Serve a JSON response containing all pipelines and their status.
 * 
 * @param pipeline_manager Pointer to the PipelineManager instance.
 * @return JSON response containing all pipeline status.
 */
crow::json::wvalue serveAllPipelines(beast::PipelineManager* pipeline_manager) {
  crow::json::wvalue value = crow::json::wvalue::list();
  uint32_t idx = 0;
  for (const auto& pipeline : pipeline_manager->getPipelines()) {
    crow::json::wvalue pipeline_item;
    pipeline_item["id"] = pipeline.id;
    pipeline_item["name"] = pipeline.name;
    pipeline_item["state"] =
        pipeline.pipeline.isRunning() ? std::string("running") : std::string("stopped");
    value[idx] = std::move(pipeline_item);
    idx++;
  }
  return value;
}

/**
 * Serve a static file.
 * 
 * @param res Response object.
 * @param html_root HTML root directory.
 * @param path Path of the file.
 */
void serveStaticFile(
    crow::response* res, const std::string_view html_root, const std::string_view path) {
  const std::string full_path = std::string(html_root) + "/" + std::string(path);
  try {
    if (std::ifstream(full_path)) {
      res->set_static_file_info(full_path);
    } else {
      res->code = 404;
    }
  } catch (const std::runtime_error& exception) {
    res->code = 500;
    res->write(exception.what());
  }
  res->end();
}

int main(int argc, char** argv) {
  // Default values
  std::string html_root = ".";
  std::string storage_folder = ".";
  uint16_t http_port = 9192;

  // Parse command line arguments
  try {
    CLI::App cli{"BEAST Compose"};
    cli.add_option("--html_root", html_root, "The HTML root directory");
    cli.add_option("--http_port", http_port, "The HTTP port to serve on");
    cli.add_option("--storage_folder", storage_folder, "The folder to keep pipeline files in");

    CLI11_PARSE(cli, argc, argv)
  } catch (const std::runtime_error& exception) {
    std::cerr << "Failed to parse arguments: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  }

  // Set up BEAST environment
  beast::PipelineManager pipeline_manager(storage_folder);

  // Set up web serving environment
  crow::SimpleApp app;
  // Set up serving routes
  try {
    CROW_ROUTE(app, "/api/v1/status").methods("GET"_method)(
        [&](const crow::request& /*req*/) {
          return serveStatus();
        });
    CROW_ROUTE(app, "/api/v1/pipelines/new").methods("POST"_method)(
        [&](const crow::request& req) {
          return serveNewPipeline(req, &pipeline_manager);
        });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>").methods("GET"_method)(
        [&](const crow::request& /*req*/, int32_t pipeline_id) {
          return servePipelineById(&pipeline_manager, pipeline_id);
        });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>/<path>").methods("GET"_method, "POST"_method)(
        [&](const crow::request& req, int32_t pipeline_id, const std::string_view path) {
          return servePipelineAction(req, &pipeline_manager, pipeline_id, path);
        });
    CROW_ROUTE(app, "/api/v1/pipelines").methods("GET"_method)(
        [&](const crow::request& /*req*/) {
          return serveAllPipelines(&pipeline_manager);
        });
    CROW_ROUTE(app, "/<path>").methods("GET"_method)(
        [&](const crow::request& /*req*/, crow::response& res, const std::string_view path) {
          serveStaticFile(&res, html_root, path);
        });
    CROW_ROUTE(app, "/").methods("GET"_method)(
        [&](const crow::request& /*req*/, crow::response& res) {
          serveStaticFile(&res, html_root, "index.html");
        });
  } catch (const std::runtime_error& exception) {
    std::cerr << "Failed to set up server routes: " << exception.what() << std::endl;
  }

  // Start application
  try {
    app.port(http_port).multithreaded().run();
  } catch(const std::runtime_error& exception) {
    std::cerr << "Failed to start REST server: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  }

  // Quit
  return EXIT_SUCCESS;
}
