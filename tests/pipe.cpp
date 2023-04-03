#include <catch2/catch.hpp>

#include <beast/beast.hpp>

class MockPipe : public beast::Pipe {
 public:
  explicit MockPipe(uint32_t max_candidates) : beast::Pipe(max_candidates) {}

  void execute() override {}
};

TEST_CASE("pipe_has_space_until_max_input_population_reached", "pipe") {
  const std::vector<unsigned char> candidate = {};
  const int32_t max_population = 10;

  MockPipe pipe(max_population);

  for (uint32_t idx = 0; idx < max_population; ++idx) {
    REQUIRE(pipe.hasSpace() == true);
    pipe.addInput(candidate);
  }

  REQUIRE(pipe.hasSpace() == false);
}

TEST_CASE("drawing_input_from_pipe_without_candidates_throws", "pipe") {
  MockPipe pipe(1);
  REQUIRE_THROWS_AS(pipe.drawInput(0), std::underflow_error);
}

TEST_CASE("drawing_output_from_pipe_without_candidates_throws", "pipe") {
  MockPipe pipe(1);
  REQUIRE_THROWS_AS(pipe.drawOutput(0), std::underflow_error);
}
