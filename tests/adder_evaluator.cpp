// Catch2
#include <catch2/catch.hpp>

// Standard
#include <cstdint>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("AdderEvaluator returns 0 for a program that never produces output") {
  beast::AdderEvaluator evaluator(/*trial_count=*/4, /*value_range=*/10,
                                  /*max_steps_per_trial=*/50);
  beast::Program empty(/*space=*/32);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/16, /*string_table_count=*/0,
                           /*max_string_size=*/0);
  CHECK(evaluator.evaluate(session) == Approx(0.0));
}

TEST_CASE("AdderEvaluator parameters are clamped to sane values") {
  beast::AdderEvaluator zero(/*trial_count=*/0, /*value_range=*/0,
                             /*max_steps_per_trial=*/0);
  CHECK(zero.getTrialCount() == 1);
  CHECK(zero.getValueRange() == 1);
  CHECK(zero.getMaxStepsPerTrial() == 1000);
}

TEST_CASE("MaximumEvaluator clamps the input count into a sane range") {
  beast::MaximumEvaluator below(/*input_count=*/0, /*trial_count=*/2,
                                /*value_range=*/10, /*max_steps_per_trial=*/50);
  CHECK(below.getInputCount() == 2);

  beast::MaximumEvaluator above(/*input_count=*/9999, /*trial_count=*/2,
                                /*value_range=*/10, /*max_steps_per_trial=*/50);
  CHECK(above.getInputCount() == 16);
}

TEST_CASE("MaximumEvaluator returns 0 for a program that never produces output") {
  beast::MaximumEvaluator evaluator(/*input_count=*/3, /*trial_count=*/3,
                                    /*value_range=*/10, /*max_steps_per_trial=*/50);
  beast::Program empty(/*space=*/32);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/16, /*string_table_count=*/0,
                           /*max_string_size=*/0);
  CHECK(evaluator.evaluate(session) == Approx(0.0));
}
