// Catch2
#include <catch2/catch.hpp>

// BEAST
#include <beast/beast.hpp>

class MockPipe : public beast::Pipe {
 public:
  explicit MockPipe(uint32_t max_candidates) : beast::Pipe(max_candidates, 1, 1) {}

  void execute() override {}

  void addOutput(uint32_t slot_index, const std::vector<unsigned char>& candidate) {
    storeOutput(slot_index, {candidate, 0.0});
  }
};

TEST_CASE("pipe") {
  SECTION("pipe_has_space_until_max_input_population_reached") {
    const std::vector<unsigned char> candidate = {};
    const int32_t max_population = 10;

    MockPipe pipe(max_population);

    for (uint32_t idx = 0; idx < max_population; ++idx) {
      REQUIRE(pipe.inputHasSpace(0) == true);
      pipe.addInput(0, candidate);
    }

    REQUIRE(pipe.inputHasSpace(0) == false);
  }

  SECTION("drawing_input_from_pipe_without_candidates_throws") {
    MockPipe pipe(1);
    REQUIRE_THROWS_AS(pipe.drawInput(0), std::underflow_error);
  }

  SECTION("drawing_output_from_pipe_without_candidates_throws") {
    MockPipe pipe(1);
    REQUIRE_THROWS_AS(pipe.drawOutput(0), std::underflow_error);
  }

  SECTION("default_input_slot_count_is_one") {
    MockPipe pipe(1);
    REQUIRE(pipe.getInputSlotCount() == 1);
  }

  SECTION("default_output_slot_count_is_one") {
    MockPipe pipe(1);
    REQUIRE(pipe.getOutputSlotCount() == 1);
  }

  SECTION("input_slot_amount_equals_added_candidates") {
    const uint32_t max_population = 12;
    const uint32_t population = 7;

    MockPipe pipe(max_population);
    const std::vector<unsigned char> candidate = {};

    for (uint32_t idx = 0; idx < population; ++idx) {
      pipe.addInput(0, candidate);
    }

    REQUIRE(pipe.getInputSlotAmount(0) == population);
  }

  SECTION("Having max population input candidates means inputs are saturated") {
    const uint32_t max_population = 12;

    MockPipe pipe(max_population);
    REQUIRE(pipe.getInputSlotAmount(0) == 0);
    REQUIRE(pipe.inputsAreSaturated() == false);

    const std::vector<unsigned char> candidate = {};

    for (uint32_t idx = 0; idx < max_population; ++idx) {
      pipe.addInput(0, candidate);
    }

    REQUIRE(pipe.getInputSlotAmount(0) == max_population);
    REQUIRE(pipe.inputsAreSaturated() == true);
  }

  SECTION("Having max population output candidates means outputs are saturated") {
    const uint32_t max_population = 15;

    MockPipe pipe(max_population);
    REQUIRE(pipe.getOutputSlotAmount(0) == 0);
    REQUIRE(pipe.outputsAreSaturated() == false);

    const std::vector<unsigned char> candidate = {};

    for (uint32_t idx = 0; idx < max_population; ++idx) {
      pipe.addOutput(0, candidate);
    }

    REQUIRE(pipe.getOutputSlotAmount(0) == max_population);
    REQUIRE(pipe.outputsAreSaturated() == true);
  }
}
