#include <beast/evaluators/identity_evaluator.hpp>

// Standard
#include <algorithm>
#include <cstdint>
#include <vector>

namespace beast {

namespace {

// Clamp the requested width to a sane range. 1 is the smallest meaningful copy task,
// 16 is plenty for any curriculum stage I can think of and keeps the VM session bounded.
// SHA-256 round uses width 8, so the default sits there.
constexpr uint32_t clampWidth(uint32_t requested) noexcept {
  if (requested == 0U) {
    return 1U;
  }
  if (requested > 16U) {
    return 16U;
  }
  return requested;
}

constexpr uint32_t kRngSeed = 0x1DE17717U;

} // namespace

IdentityEvaluator::IdentityEvaluator(uint32_t trial_count, uint32_t width,
                                     uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed),
      width_(clampWidth(width)) {}

uint32_t IdentityEvaluator::getWidth() const noexcept { return width_; }

uint32_t IdentityEvaluator::inputCount() const { return width_; }

uint32_t IdentityEvaluator::outputCount() const { return width_; }

void IdentityEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                        std::vector<uint32_t>& outputs,
                                        uint32_t /*trial_id*/) {
  std::copy(inputs.begin(), inputs.end(), outputs.begin());
}

} // namespace beast
