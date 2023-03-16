// Standard
#include <cstdlib>
#include <iostream>
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

  // Set up web serving environment
  crow::SimpleApp app;

  // Set up serving routes
  try {
    CROW_ROUTE(app, "/api/v1/status")
        ([](const crow::request& /*req*/) {
           crow::json::wvalue value;
           value["version"] = "0.1.x";
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
