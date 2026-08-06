#include <beast/evaluators/rotate_evaluator.hpp>

// Standard
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xB077A7E5U;

// Clamp the rotation amount into [1, 31]. A 0-bit rotation is the identity (handled by
// IdentityEvaluator), a 32-bit rotation is also the identity, and amounts > 31 are
// undefined for std::rotr / our hand-rolled rotr. Clamping rather than throwing lets the
// JSON-loaded value be sloppy without crashing the server.
constexpr uint32_t clampAmount(uint32_t requested) noexcept {
  if (requested == 0U) {
    return 1U;
  }
  if (requested > 31U) {
    return 31U;
  }
  return requested;
}

constexpr uint32_t rotr(uint32_t value, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return value;
  }
  return (value >> amount) | (value << (32U - amount));
}

constexpr uint32_t rotl(uint32_t value, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return value;
  }
  return (value << amount) | (value >> (32U - amount));
}

} // namespace

RotateEvaluator::Direction RotateEvaluator::parseDirection(const std::string& name) {
  if (name == "right") {
    return Direction::Right;
  }
  if (name == "left") {
    return Direction::Left;
  }
  throw std::invalid_argument("Unknown RotateEvaluator direction: " + name);
}

std::string RotateEvaluator::directionName(Direction direction) {
  return direction == Direction::Left ? "left" : "right";
}

RotateEvaluator::RotateEvaluator(uint32_t trial_count, uint32_t amount, Direction direction,
                                 uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed),
      amount_(clampAmount(amount)),
      direction_(direction) {}

uint32_t RotateEvaluator::getAmount() const noexcept { return amount_; }

RotateEvaluator::Direction RotateEvaluator::getDirection() const noexcept { return direction_; }

uint32_t RotateEvaluator::inputCount() const { return 1U; }

uint32_t RotateEvaluator::outputCount() const { return 1U; }

void RotateEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                      std::vector<uint32_t>& outputs, uint32_t /*trial_id*/) {
  outputs.at(0) = direction_ == Direction::Right ? rotr(inputs.at(0), amount_)
                                                 : rotl(inputs.at(0), amount_);
}

} // namespace beast
