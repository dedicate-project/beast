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

  pipelines.push_back({"Some Pipeline", beast::Pipeline()});
  pipelines.push_back({"Some other Pipeline", beast::Pipeline()});

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
    CROW_ROUTE(app, "/api/v1/pipelines")
        ([pipelines](const crow::request& /*req*/) {
           crow::json::wvalue value;
           uint32_t idx = 0;
           for (const auto& pipeline : pipelines) {
             crow::json::wvalue pipeline_item;
             pipeline_item["id"] = idx;
             pipeline_item["name"] = pipeline.name;
             value[idx] = std::move(pipeline_item);
             idx++;
           }
           return value;
         });
    CROW_ROUTE(app, "/<path>")
        ([html_root](const crow::request& /*req*/, crow::response& res, const std::string& path) {
           const std::string full_path = html_root + "/" + path;
           serveFile(&res, full_path);
           res.end();
         });
    CROW_ROUTE(app, "/")
        ([html_root](const crow::request& /*req*/, crow::response& res) {
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
