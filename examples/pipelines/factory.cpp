// Standard
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

// BEAST
#include <beast/beast.hpp>

int main(int /*argc*/, char** /*argv*/) {
  std::cout << "Using BEAST library version " << beast::getVersionString() << std::endl;

  beast::Pipeline pipeline;

  std::shared_ptr<beast::RandomProgramFactory> factory =
      std::make_shared<beast::RandomProgramFactory>();
  std::shared_ptr<beast::ProgramFactoryPipe> factory_pipe =
      std::make_shared<beast::ProgramFactoryPipe>(10, 50, 5, 10, 25, factory);

  pipeline.addPipe("factory", factory_pipe);

  std::shared_ptr<beast::NullSinkPipe> sink_pipe = std::make_shared<beast::NullSinkPipe>();

  pipeline.addPipe("sink", sink_pipe);

  pipeline.connectPipes(factory_pipe, 0, sink_pipe, 0, 10);

  std::cout << "Starting Pipeline" << std::endl;
  pipeline.start();

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return EXIT_SUCCESS;
}
