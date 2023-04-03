// Standard
#include <chrono>

// Catch2
#include <catch2/catch.hpp>

// BEAST
#include <beast/beast.hpp>

namespace {
class MockPipe : public beast::EvolutionPipe {
 public:
  MockPipe() : EvolutionPipe(1) {}

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& /*program_data*/) override { return 0.0; }
};
} // namespace

TEST_CASE("adding_pipes_to_pipelines_and_retrieving_them_works_correctly", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);

  const auto& pipes = pipeline.getPipes();
  auto iter = pipes.begin();
  REQUIRE(iter->pipe == pipe0);
  iter++;
  REQUIRE(iter->pipe == pipe1);
}

TEST_CASE("adding_a_pipe_to_a_pipeline_twice_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe);

  bool threw = false;
  try {
    pipeline.addPipe(pipe);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_a_connection_with_source_pipe_not_in_pipeline_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);

  bool threw = false;
  try {
    pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_a_connection_with_destination_pipe_not_in_pipeline_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();

  bool threw = false;
  try {
    pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_a_connection_with_either_pipe_not_in_pipeline_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();

  bool threw = false;
  try {
    pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_connections_with_the_same_source_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);
  std::shared_ptr<beast::Pipe> pipe2 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe2);

  pipeline.connectPipes(pipe0, 0, pipe1, 1, 1);

  bool threw = false;
  try {
    pipeline.connectPipes(pipe0, 0, pipe2, 2, 1);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_connections_with_the_same_destination_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);
  std::shared_ptr<beast::Pipe> pipe2 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe2);

  pipeline.connectPipes(pipe0, 0, pipe2, 2, 1);

  bool threw = false;
  try {
    pipeline.connectPipes(pipe1, 1, pipe2, 2, 1);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_the_same_connection_twice_throws", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);

  pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);

  bool threw = false;
  try {
    pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_a_connection_and_retrieving_it_works", "pipeline") {
  beast::Pipeline pipeline;
  std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe0);
  std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
  pipeline.addPipe(pipe1);

  pipeline.connectPipes(pipe0, 0, pipe1, 1, 1);

  const auto& connections = pipeline.getConnections();
  REQUIRE(connections.size() == 1);

  const auto& connection = *connections.begin();
  REQUIRE(connection.source_pipe == pipe0);
  REQUIRE(connection.source_slot_index == 0);
  REQUIRE(connection.destination_pipe == pipe1);
  REQUIRE(connection.destination_slot_index == 1);
}
