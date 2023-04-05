// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("NullSinkPipe") {
  beast::NullSinkPipe pipe;

  SECTION("Pipe has one input") { REQUIRE(pipe.getInputSlotCount() == 1); }

  SECTION("Pipe has no outputs") { REQUIRE(pipe.getOutputSlotCount() == 0); }

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
}