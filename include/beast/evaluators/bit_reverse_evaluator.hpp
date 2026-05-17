#ifndef BEAST_EVALUATORS_BIT_REVERSE_EVALUATOR_HPP_
#define BEAST_EVALUATORS_BIT_REVERSE_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class BitReverseEvaluator
 * @brief Scores a program on reversing the bit order of a single input word
 *
 * Primitive-gym task. Reference function:
 *   `output[0] = reverse_bits(input[0])`
 * where bit 0 of input becomes bit 31 of output, bit 1 becomes bit 30, etc. Solvable
 * in ~10-30 opcodes with shift+mask, or via the classic Hacker's Delight bit-swap
 * pattern at log(32)=5 levels of XOR/shift.
 *
 * Scoring: bit-Hamming distance. The output and reference are both full 32-bit
 * bitfields, so bit-Hamming is the natural gradient -- each correctly-reversed bit
 * contributes 1/32 of the score, giving the GA a smooth slope to climb from random.
 * Random output scores ~0.5 in expectation (half the bits match by chance), so the
 * "convergence floor" is around 0.5 and any score above ~0.7 indicates the genome
 * has discovered at least the high-bit-counting half of the reversal.
 *
 * Useful as a subroutine for tasks that need to read multi-bit features from the
 * LSB end of a word but emit them MSB-first (or vice versa), such as encoding a
 * direction-of-travel bitmask consumed by another component.
 */
class BitReverseEvaluator : public BitDistanceEvaluator {
 public:
  explicit BitReverseEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial);

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;
};

} // namespace beast

#endif // BEAST_EVALUATORS_BIT_REVERSE_EVALUATOR_HPP_
