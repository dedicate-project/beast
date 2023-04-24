// Catch2
#include <catch2/catch.hpp>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("MazeEvaluator") {
  SECTION("Parameters are initialized and returned correctly") {
    const uint32_t rows = 27;
    const uint32_t cols = 15;
    const double difficulty = 0.64;
    const uint32_t max_steps = 230;

    beast::MazeEvaluator evaluator(rows, cols, difficulty, max_steps);

    REQUIRE(evaluator.getRows() == rows);
    REQUIRE(evaluator.getCols() == cols);
    REQUIRE(evaluator.getDifficulty() == difficulty);
    REQUIRE(evaluator.getMaxSteps() == max_steps);
  }
}
