#ifndef BEAST_EVALUATORS_ADDER_EVALUATOR_HPP_
#define BEAST_EVALUATORS_ADDER_EVALUATOR_HPP_

// Standard
#include <cstdint>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class AdderEvaluator
 * @brief Scores a program on its ability to compute the sum of two numbers
 *
 * Each call to `evaluate()` runs the supplied program against `trial_count` randomised
 * pairs `(a, b)` and accumulates a per-trial score derived from the program's output:
 * a perfect match scores 1.0, otherwise the score decays smoothly with the absolute
 * error scaled by the magnitude of the target. The returned score is the average over
 * all trials, kept in [0, 1] so it composes cleanly with `AggregationEvaluator`.
 *
 * Variable layout
 * - Var 0: input `a`
 * - Var 1: input `b`
 * - Var 2: input `trial_id` (incremented each trial so the program can detect when a
 *   new round started without having to compare the literal values of `a` and `b`)
 * - Var 3: output `sum`
 *
 * Construction
 * - `trial_count`:        how many (a, b) pairs to evaluate per call (default 8).
 * - `value_range`:        absolute bound on the random values; pairs are drawn from
 *                         [-value_range, +value_range] inclusive.
 * - `max_steps_per_trial` step budget per trial. Programs that don't produce an output
 *                         within the budget for a given trial score 0.0 for it but the
 *                         remaining trials still run.
 */
class AdderEvaluator : public Evaluator {
 public:
  AdderEvaluator(uint32_t trial_count, int32_t value_range, uint32_t max_steps_per_trial);

  [[nodiscard]] double evaluate(const VmSession& session) override;

  [[nodiscard]] uint32_t getTrialCount() const noexcept;
  [[nodiscard]] int32_t getValueRange() const noexcept;
  [[nodiscard]] uint32_t getMaxStepsPerTrial() const noexcept;

 private:
  const uint32_t trial_count_;
  const int32_t value_range_;
  const uint32_t max_steps_per_trial_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_ADDER_EVALUATOR_HPP_
