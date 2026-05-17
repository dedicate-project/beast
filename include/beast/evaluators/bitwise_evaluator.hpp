#ifndef BEAST_EVALUATORS_BITWISE_EVALUATOR_HPP_
#define BEAST_EVALUATORS_BITWISE_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <string>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class BitwiseEvaluator
 * @brief Scores a program on computing a two-input (or one-input) bitwise operation
 *
 * Curriculum stage that teaches the GA to use a specific bitwise primitive in
 * isolation. For two-input operations (XOR, AND, OR) the program reads inputs at
 * vars 0 and 1; for the one-input operation (NOT) it reads only var 0. The expected
 * output is a single word at the output slot.
 *
 * Why we need this: random programs from the factory don't bias toward bitwise ops
 * unless the `opcode_weights` map says so, and even with bias the first useful XOR
 * tends to take a while to discover. Pulling a population that's already nailed XOR
 * into the Sigma / Ch / Maj stages via seeding is a meaningful head start.
 */
class BitwiseEvaluator : public BitDistanceEvaluator {
 public:
  /**
   * @brief Which bitwise operation to score against.
   *
   * XOR, AND, OR each take two inputs; NOT takes one. Other useful operations
   * (right-shift, left-shift) are intentionally NOT here -- they belong to the
   * RotateEvaluator family where the rotation/shift amount is an explicit knob, not
   * a runtime input.
   */
  enum class Operation : uint8_t { Xor = 0, And = 1, Or = 2, Not = 3 };

  static Operation parseOperation(const std::string& name);
  static std::string operationName(Operation op);

  BitwiseEvaluator(uint32_t trial_count, Operation op, uint32_t max_steps_per_trial);

  [[nodiscard]] Operation getOperation() const noexcept;

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;

 private:
  const Operation operation_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_BITWISE_EVALUATOR_HPP_
