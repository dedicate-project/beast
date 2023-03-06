// Standard
#include <chrono>

// Catch2
#include <catch2/catch.hpp>

// BEAST
#include <beast/beast.hpp>

namespace {
class MockPipe : public beast::Pipe {
 public:
  MockPipe() : Pipe(1) {}

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& /*program_data*/) override {
    return 0.0;
  }
};
}  // namespace

TEST_CASE("adding_pipes_to_pipelines_and_retrieving_them_works_correctly", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);

  const auto& pipes = pipeline.getPipes();
  auto iter = pipes.begin();
  REQUIRE(*iter == pipe0);
  iter++;
  REQUIRE(*iter == pipe1);
}

TEST_CASE("adding_a_pipe_to_a_pipeline_twice_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe);

  bool threw = false;
  try {
    pipeline.addPipe(pipe);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}
