#ifndef BEAST_EVALUATORS_ROTATE_EVALUATOR_HPP_
#define BEAST_EVALUATORS_ROTATE_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <string>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class RotateEvaluator
 * @brief Scores a program on rotating a single input word by a fixed amount
 *
 * Curriculum stage that teaches the GA to use the RotateVariable opcodes with a
 * specific amount. SHA-256 uses several distinct rotation amounts (2, 6, 7, 11, 13,
 * 17, 18, 19, 22, 25) and each is its own discrete skill -- a population that's good
 * at rotr(x, 2) might still flounder at rotr(x, 11). Instantiate one RotateEvaluator
 * per amount you care about, or run them in parallel.
 *
 * The rotation amount is fixed per evaluator (not a runtime input). This is
 * intentional: SHA-256 uses *specific* constants in its rotations, so the program
 * encodes them as literals rather than reading them from variables.
 */
class RotateEvaluator : public BitDistanceEvaluator {
 public:
  enum class Direction : uint8_t { Right = 0, Left = 1 };

  static Direction parseDirection(const std::string& name);
  static std::string directionName(Direction direction);

  RotateEvaluator(uint32_t trial_count, uint32_t amount, Direction direction,
                  uint32_t max_steps_per_trial);

  [[nodiscard]] uint32_t getAmount() const noexcept;
  [[nodiscard]] Direction getDirection() const noexcept;

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;

 private:
  const uint32_t amount_;
  const Direction direction_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_ROTATE_EVALUATOR_HPP_
