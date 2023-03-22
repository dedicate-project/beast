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

struct PipelineDescriptor {
  uint32_t id;
  std::string name;
  beast::Pipeline pipeline;
};

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
  uint16_t http_port = 9192;

  // Parse command line arguments
  try {
    CLI::App cli{"BEAST Serve"};

    cli.add_option("--html_root", html_root, "HTML root directory");
    cli.add_option("--http_port", html_root, "The HTTP port to serve on");

    CLI11_PARSE(cli, argc, argv)
  } catch(const std::runtime_error& exception) {
    std::cerr << "Failed to parse arguments: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  } catch(...) {
    std::cerr << "Failed to parse arguments due to an unknown issue" << std::endl;
    return EXIT_FAILURE;
  }

  // Set up BEAST environment
  std::list<PipelineDescriptor> pipelines;

  const auto add_pipeline =
      [&pipelines](const std::string& name) -> uint32_t {
        uint32_t new_id = -1;
        std::list<PipelineDescriptor>::iterator iter;
        do {
          new_id++;
          iter = std::find_if(pipelines.begin(), pipelines.end(),
                              [new_id](const auto& pipeline) { return pipeline.id == new_id; });
        } while(iter != pipelines.end());
        pipelines.push_back({new_id, name, beast::Pipeline()});
        return new_id;
      };

  add_pipeline("Some Pipeline");
  add_pipeline("Some other Pipeline");

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
        ([&pipelines, &add_pipeline](const crow::request& req) {
           crow::json::wvalue value;
           const auto req_body = crow::json::load(req.body);
           value["status"] = "success";
           value["id"] = add_pipeline(static_cast<std::string>(req_body["name"]));
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>")
        ([&pipelines](const crow::request& /*req*/, int32_t id) {
           crow::json::wvalue value;
           value["id"] = id;
           auto iter = std::find_if(pipelines.begin(), pipelines.end(),
                                   [id](const auto& pipeline) { return pipeline.id == id; });
           if (iter == pipelines.end()) {
             value["status"] = "failed";
             value["error"] = "invalid_id";
           } else {
             value["state"] = iter->pipeline.isRunning() ? std::string("running") : std::string("stopped");
           }
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>/<path>")
        ([&pipelines](const crow::request& /*req*/, int32_t id, const std::string& path) {
           crow::json::wvalue value;
           value["id"] = id;
           auto iter = std::find_if(pipelines.begin(), pipelines.end(),
                                   [id](const auto& pipeline) { return pipeline.id == id; });
           if (iter == pipelines.end()) {
             value["status"] = "failed";
             value["error"] = "invalid_id";
           } else {
             auto& pipeline = iter->pipeline;
             if (path == "start") {
               if (!pipeline.isRunning()) {
                 try {
                   pipeline.start();
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
               if (pipeline.isRunning()) {
                 try {
                   pipeline.stop();
                   value["status"] = "success";
                 } catch(...) {
                   value["status"] = "failed";
                   value["error"] = "stop_failed";
                 }
               } else {
                 value["status"] = "failed";
                 value["error"] = "not_running";
               }
             } else {
               value["status"] = "failed";
               value["error"] = "invalid_command";
               value["command"] = path;
             }
           }
           return value;
         });
    CROW_ROUTE(app, "/api/v1/pipelines")
        ([&pipelines](const crow::request& /*req*/) {
           crow::json::wvalue value;
           uint32_t idx = 0;
           for (const auto& pipeline : pipelines) {
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
