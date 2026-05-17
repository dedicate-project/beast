#ifndef BEAST_EVALUATORS_PARITY_EVALUATOR_HPP_
#define BEAST_EVALUATORS_PARITY_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class ParityEvaluator
 * @brief Scores a program on XOR-reducing N input words to a single output
 *
 * Primitive-gym task. Reference function: `output[0] = input[0] ^ input[1] ^ ... ^
 * input[N-1]`. With the default `width=4` that's three XOR opcodes plus the
 * load/store overhead -- one of the easiest things in the whole evaluator suite to
 * evolve, which makes it a useful "is the GA stack alive at all" smoke target.
 *
 * Scoring: bit-Hamming distance per output bit. The output IS a 32-bit accumulator
 * (the parity is computed per bit position across the inputs), so bit-Hamming is the
 * right gradient here -- it directly rewards "you got bit 17 right but missed bit 4"
 * with a finer-grained signal than numeric distance would.
 *
 * Useful as a subroutine for tasks that need a quick hash-like summary of several
 * input words. The maze evaluator's perception grid is a natural candidate: parity
 * over the corners of a 3-tile-radius window gives a one-bit signature of the local
 * topology that downstream logic can branch on.
 */
class ParityEvaluator : public BitDistanceEvaluator {
 public:
  ParityEvaluator(uint32_t trial_count, uint32_t width, uint32_t max_steps_per_trial);

  [[nodiscard]] uint32_t getWidth() const noexcept;

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;

 private:
  const uint32_t width_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_PARITY_EVALUATOR_HPP_
