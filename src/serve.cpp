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

void serveFile(crow::response* res, const std::string& full_path) {
  try {
    if (std::ifstream(full_path)) {
      res->set_static_file_info(full_path);
    } else {
      res->code = 404;
    }
  } catch(...) {
    res->code = 500;
  }
}

int main(int argc, char** argv) {
  // Default values
  std::string html_root = ".";
  std::string storage_folder = ".";
  uint16_t http_port = 9192;

  // Parse command line arguments
  try {
    CLI::App cli{"BEAST Serve"};

    cli.add_option("--html_root", html_root, "The HTML root directory");
    cli.add_option("--http_port", http_port, "The HTTP port to serve on");
    cli.add_option("--storage_folder", storage_folder, "The folder to keep pipeline files in");

    CLI11_PARSE(cli, argc, argv)
  } catch(const std::runtime_error& exception) {
    std::cerr << "Failed to parse arguments: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  } catch(...) {
    std::cerr << "Failed to parse arguments due to an unknown issue" << std::endl;
    return EXIT_FAILURE;
  }

  // Set up BEAST environment
  beast::PipelineManager pipeline_manager(storage_folder);

  // Set up web serving environment
  crow::SimpleApp app;

  // Set up serving routes
  try {
    CROW_ROUTE(app, "/api/v1/status")
        ([](const crow::request& /*req*/) {
           crow::json::wvalue value;
           const auto version = beast::getVersion();
           value["version"] =
               std::to_string(version[0]) + "." +
               std::to_string(version[1]) + "." +
               std::to_string(version[2]);
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines/new")
    .methods("POST"_method)
        ([&pipeline_manager](const crow::request& req) {
           crow::json::wvalue value;
           const auto req_body = crow::json::load(req.body);
           value["status"] = "success";
           value["id"] =
               pipeline_manager.createPipeline(static_cast<std::string>(req_body["name"]));
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>")
        ([&pipeline_manager](const crow::request& /*req*/, int32_t id) {
           crow::json::wvalue value;
           value["id"] = id;
           try {
             const auto pipeline = pipeline_manager.getPipelineById(id);
             value["state"] =
                 pipeline.pipeline.isRunning() ? std::string("running") : std::string("stopped");
           } catch(...) {
             value["status"] = "failed";
             value["error"] = "invalid_id";
           }
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>/<path>")
    .methods("POST"_method, "GET"_method)
        ([&pipeline_manager](const crow::request& req, int32_t id, const std::string& path) {
           crow::json::wvalue value;
           value["id"] = id;
           try {
             auto pipeline = pipeline_manager.getPipelineById(id);
             const bool running = pipeline.pipeline.isRunning();

             if (path == "start") {
               if (running) {
                 try {
                   pipeline.pipeline.start();
                   value["status"] = "success";
                 } catch(...) {
                   value["status"] = "failed";
                   value["error"] = "start_failed";
                 }
               } else {
                 value["status"] = "failed";
                 value["error"] = "already_running";
               }
             } else if (path == "stop") {
               if (running) {
                 try {
                   pipeline.pipeline.stop();
                   value["status"] = "success";
                 } catch(...) {
                   value["status"] = "failed";
                   value["error"] = "stop_failed";
                 }
               } else {
                 value["status"] = "failed";
                 value["error"] = "not_running";
               }
             } else if (path == "update") {
               if (req.get_header_value("Content-Type") == "application/json") {
                 // Parse the JSON data in the request body
                 try {
                   const auto req_body = crow::json::load(req.body);
                   const std::string action = static_cast<std::string>(req_body["action"]);

                   if (action == "change_name") {
                     const std::string new_name = static_cast<std::string>(req_body["name"]);
                     pipeline_manager.updatePipelineName(pipeline.id, new_name);
                   } else {
                     value["status"] = "failed";
                     value["action"] = action;
                     value["error"] = "invalid_action";
                   }
                 } catch(...) {
                   value["status"] = "failed";
                   value["error"] = "invalid_request";
                 }
               } else {
                 value["status"] = "failed";
                 value["error"] = "invalid_request";
               }
             } else if (path == "delete") {
               pipeline.pipeline.stop();
               pipeline_manager.deletePipeline(pipeline.id);
             } else {
               value["status"] = "failed";
               value["error"] = "invalid_command";
               value["command"] = path;
             }
           } catch(...) {
             value["status"] = "failed";
             value["error"] = "invalid_id";
           }
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines")
        ([&pipeline_manager](const crow::request& /*req*/) {
           crow::json::wvalue value = crow::json::wvalue::list();
           uint32_t idx = 0;
           for (const auto& pipeline : pipeline_manager.getPipelines()) {
             crow::json::wvalue pipeline_item;
             pipeline_item["id"] = pipeline.id;
             pipeline_item["name"] = pipeline.name;
             pipeline_item["state"] =
                 pipeline.pipeline.isRunning() ? std::string("running") : std::string("stopped");
             value[idx] = std::move(pipeline_item);
             idx++;
           }
           return value;
         });
    CROW_ROUTE(app, "/<path>")
        ([&html_root](const crow::request& /*req*/, crow::response& res, const std::string& path) {
           const std::string full_path = html_root + "/" + path;
           serveFile(&res, full_path);
           res.end();
         });
    CROW_ROUTE(app, "/")
        ([&html_root](const crow::request& /*req*/, crow::response& res) {
           const std::string full_path = html_root + "/index.html";
           serveFile(&res, full_path);
           res.end();
         });
  } catch(...) {
    std::cerr << "Failed to set up server routes" << std::endl;
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
