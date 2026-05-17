#ifndef BEAST_EVALUATORS_POPCOUNT_EVALUATOR_HPP_
#define BEAST_EVALUATORS_POPCOUNT_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class PopcountEvaluator
 * @brief Scores a program on counting the set bits in a single input word
 *
 * Primitive-gym task. Reference function: `output[0] = popcount(input[0])` -- a 32-bit
 * input word, an integer count in 0..32 written to the first output slot. Solvable in
 * a few opcodes (loop-and-test, or the Hacker's Delight branchless trick). Tiny
 * starting program size, very fast convergence (seconds-to-minutes), excellent
 * sanity-check that the GA stack as a whole is healthy.
 *
 * Scoring: numeric distance, NOT bit-Hamming. The reference output is a small integer
 * (0..32); a bit-Hamming gradient would award "the answer is 0x80 but you wrote 0x00"
 * almost the same score as "the answer is 17 but you wrote 18" -- both are off by a
 * single bit. Numeric distance ("you're 1 off" >> "you're 128 off") is the gradient
 * the GA actually wants to climb.
 *
 * Useful as a subroutine for any downstream task that needs to count set bits inside
 * a perception window, a bitmask, or an accumulator. The maze evaluator's perception
 * tiles are encoded as small integers, so a popcount subroutine acts as a "how many
 * obstacles are visible" feature extractor.
 */
class PopcountEvaluator : public BitDistanceEvaluator {
 public:
  explicit PopcountEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial);

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;
  [[nodiscard]] double scoreWord(uint32_t expected, uint32_t observed) const noexcept override;
};

} // namespace beast

#endif // BEAST_EVALUATORS_POPCOUNT_EVALUATOR_HPP_
