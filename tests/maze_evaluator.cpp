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

  SECTION("A very small max_steps short-circuits evaluation of an empty program") {
    // Regression test: previously max_steps_ was ignored and evaluate() always burnt 10000
    // VM steps before returning. With a 1-step budget an empty program (which produces no
    // move output) should fall through to the timed-out branch within a couple of VM steps.
    // We don't measure wall time here, just verify evaluate() returns the timed-out sentinel
    // (0.0 when no moves were made) instead of crashing or running for ~10000 steps.
    beast::MazeEvaluator evaluator(/*rows=*/8, /*cols=*/8, /*difficulty=*/0.1, /*max_steps=*/1);
    beast::Program empty_program(64);
    beast::VmSession session(empty_program, /*variable_count=*/64,
                             /*string_table_count=*/0, /*max_string_size=*/0);
    REQUIRE(evaluator.evaluate(session) == 0.0);
  }
}
