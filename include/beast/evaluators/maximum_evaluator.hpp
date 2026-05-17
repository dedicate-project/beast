#ifndef BEAST_EVALUATORS_MAXIMUM_EVALUATOR_HPP_
#define BEAST_EVALUATORS_MAXIMUM_EVALUATOR_HPP_

// Standard
#include <cstdint>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class MaximumEvaluator
 * @brief Scores a program on its ability to return the maximum of N input values
 *
 * The program is shown N random integers as input variables and must produce the
 * maximum on the configured output variable. The evaluator runs `trial_count`
 * randomised trials per call and returns the average per-trial score (1.0 = correct
 * maximum, smooth linear decay scaled by the spread of the trial's values).
 *
 * Variable layout
 * - Var 0..N-1: input values
 * - Var N:     input `trial_id` (lets the program detect a fresh round without
 *              having to compare values directly)
 * - Var N+1:   output `maximum`
 *
 * Construction
 * - `input_count`:        how many integers to compare (2..16, default 3).
 * - `trial_count`:        how many randomised trials per evaluate() call.
 * - `value_range`:        absolute bound on each input; drawn from
 *                         [-value_range, +value_range] inclusive.
 * - `max_steps_per_trial` per-trial step budget.
 */
class MaximumEvaluator : public Evaluator {
 public:
  MaximumEvaluator(uint32_t input_count, uint32_t trial_count, int32_t value_range,
                   uint32_t max_steps_per_trial);

  [[nodiscard]] double evaluate(const VmSession& session) override;

  [[nodiscard]] uint32_t getInputCount() const noexcept;
  [[nodiscard]] uint32_t getTrialCount() const noexcept;
  [[nodiscard]] int32_t getValueRange() const noexcept;
  [[nodiscard]] uint32_t getMaxStepsPerTrial() const noexcept;

 private:
  const uint32_t input_count_;
  const uint32_t trial_count_;
  const int32_t value_range_;
  const uint32_t max_steps_per_trial_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_MAXIMUM_EVALUATOR_HPP_
