#ifndef BEAST_EVALUATORS_SHA256_MAJ_EVALUATOR_HPP_
#define BEAST_EVALUATORS_SHA256_MAJ_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class Sha256MajEvaluator
 * @brief Scores a program on the SHA-256 "majority" function
 *
 * Maj(x, y, z) = (x AND y) XOR (x AND z) XOR (y AND z)
 *
 * Bit-by-bit, each output bit is set when at least two of the three corresponding
 * input bits are set -- the "majority" vote. Three input words (vars 0, 1, 2), one
 * output word (var 4). The reference function is 5 bitwise opcodes in BEAST.
 */
class Sha256MajEvaluator : public BitDistanceEvaluator {
 public:
  Sha256MajEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial);

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;
};

} // namespace beast

#endif // BEAST_EVALUATORS_SHA256_MAJ_EVALUATOR_HPP_
