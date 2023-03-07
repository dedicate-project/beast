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

int main(int argc, char** argv) {
  std::string html_root = ".";

  CLI::App cli{"BEAST Serve"};
  CLI::Option* opt = nullptr;
  try {
    opt = cli.add_option("--html_root", html_root, "HTML root directory");
  } catch(const std::runtime_error& exception) {
    std::cerr << "Failed to prepare CLI parser: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  }
  static_cast<void>(opt);

  CLI11_PARSE(cli, argc, argv);

  crow::SimpleApp app;

  CROW_ROUTE(app, "/<path>")
      ([&](const crow::request& /*req*/, crow::response& res, const std::string& path) {
         const std::string full_path = html_root + "/" + path;
         if (std::ifstream(full_path)) {
           res.set_static_file_info(full_path);
         } else {
           res.code = 404;
         }
         res.end();
       });

  try {
    app.port(18080).multithreaded().run();
  } catch(const std::runtime_error& exception) {
    std::cerr << "Failed to start REST server: " << exception.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
