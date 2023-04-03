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
 * Serve a static file.
 *
 * @param res Response object.
 * @param html_root HTML root directory.
 * @param path Path of the file.
 */
void serveStaticFile(crow::response* res, const std::string_view html_root,
                     const std::string_view path) {
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
  beast::PipelineServer pipeline_server(storage_folder);

  // Set up web serving environment
  crow::SimpleApp app;
  // Set up serving routes
  try {
    CROW_ROUTE(app, "/api/v1/status").methods("GET"_method)([](const crow::request& /*req*/) {
      return beast::PipelineServer::serveStatus();
    });
    CROW_ROUTE(app, "/api/v1/pipelines/new")
        .methods("POST"_method)([&pipeline_server](const crow::request& req) {
          return pipeline_server.serveNewPipeline(req);
        });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>")
        .methods("GET"_method)(
            [&pipeline_server](const crow::request& /*req*/, int32_t pipeline_id) {
              return pipeline_server.servePipelineById(static_cast<uint32_t>(pipeline_id));
            });
    CROW_ROUTE(app, "/api/v1/pipelines/<int>/<path>")
        .methods("GET"_method, "POST"_method)([&pipeline_server](const crow::request& req,
                                                                 int32_t pipeline_id,
                                                                 const std::string_view path) {
          return pipeline_server.servePipelineAction(req, static_cast<uint32_t>(pipeline_id), path);
        });
    CROW_ROUTE(app, "/api/v1/pipelines")
        .methods("GET"_method)([&pipeline_server](const crow::request& /*req*/) {
          return pipeline_server.serveAllPipelines();
        });
    CROW_ROUTE(app, "/<path>")
        .methods("GET"_method)(
            [&html_root](const crow::request& /*req*/,
                         crow::response& res,
                         const std::string_view path) { serveStaticFile(&res, html_root, path); });
    CROW_ROUTE(app, "/").methods("GET"_method)(
        [&html_root](const crow::request& /*req*/, crow::response& res) {
          serveStaticFile(&res, html_root, "index.html");
        });
  } catch (const std::runtime_error& exception) {
    std::cerr << "Failed to set up server routes: " << exception.what() << std::endl;
  }

  // Start application
  try {
    app.port(http_port).multithreaded().run();
  } catch (const std::runtime_error& exception) {
    std::cerr << "Failed to start REST server: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  }

  // Quit
  return EXIT_SUCCESS;
}
