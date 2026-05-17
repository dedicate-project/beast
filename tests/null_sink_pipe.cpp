// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("NullSinkPipe") {
  const uint32_t max_candidates = 10;
  beast::NullSinkPipe pipe(max_candidates);

  SECTION("Pipe has one input") { REQUIRE(pipe.getInputSlotCount() == 1); }

  SECTION("Pipe has no outputs") { REQUIRE(pipe.getOutputSlotCount() == 0); }

  SECTION("Pipe has the right amount of max_candidates") {
    REQUIRE(pipe.getMaxCandidates() == max_candidates);
  }

  SECTION("Executing the pipe clears the input queue") {
    // Empty initially
    REQUIRE(pipe.getInputSlotAmount(0) == 0);

    // Add three data points
    std::vector<unsigned char> data{0x1, 0x2, 0x3};
    pipe.addInput(0, data);
    pipe.addInput(0, data);
    pipe.addInput(0, data);
    REQUIRE(pipe.getInputSlotAmount(0) == 3);

    // Execute pipe
    pipe.execute();
    REQUIRE(pipe.getInputSlotAmount(0) == 0);
  }

  SECTION("Sink saturates as soon as any input is present") {
    // Regression test for the deadlock-on-slow-upstream bug: if the sink only fires once its
    // input is fully saturated (size == max_candidates), it stalls indefinitely when the
    // upstream produces less than max_candidates per cycle.
    REQUIRE_FALSE(pipe.inputsAreSaturated());
    const std::vector<unsigned char> data{0xAB};
    pipe.addInput(0, data);
    REQUIRE(pipe.inputsAreSaturated());
  }
}
