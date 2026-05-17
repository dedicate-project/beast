#ifndef BEAST_EVALUATORS_MINIMUM_EVALUATOR_HPP_
#define BEAST_EVALUATORS_MINIMUM_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class MinimumEvaluator
 * @brief Scores a program on returning the smallest of N input words
 *
 * Primitive-gym task. Reference function: `output[0] = min(input[0..N-1])`. Solvable
 * with N-1 conditional compare/swap pairs, so it's a natural target for the GA to
 * discover conditional-branch opcodes (CompareLessThan, branch-on-flag, etc.) in
 * combination with variable swapping.
 *
 * Scoring: numeric distance with a per-input-range ceiling. The reference output is
 * a 32-bit integer that can take any value in 0..UINT32_MAX, so we use log2-scale
 * numeric distance to keep the gradient meaningful across the full range. A genome
 * that's "off by one" scores ~1.0; off by a power of two scores worse; entirely-wrong
 * scores ~0.0.
 *
 * Useful as a subroutine in any task with multiple candidate values to pick from,
 * including spatial-reasoning tasks like maze nav (e.g. "of the four adjacent
 * cells, which has the smallest distance-to-frontier feature?").
 */
class MinimumEvaluator : public BitDistanceEvaluator {
 public:
  MinimumEvaluator(uint32_t trial_count, uint32_t width, uint32_t max_steps_per_trial);

  [[nodiscard]] uint32_t getWidth() const noexcept;

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;
  [[nodiscard]] double scoreWord(uint32_t expected, uint32_t observed) const noexcept override;

 private:
  const uint32_t width_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_MINIMUM_EVALUATOR_HPP_
