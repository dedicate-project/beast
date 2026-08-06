#include <beast/evaluators/parity_evaluator.hpp>

// Standard
#include <cstdint>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xC0117C0DU;
constexpr uint32_t kMinWidth = 2U;
constexpr uint32_t kMaxWidth = 8U;
constexpr uint32_t kDefaultWidth = 4U;

constexpr uint32_t clampWidth(uint32_t requested) noexcept {
  if (requested == 0U) {
    return kDefaultWidth;
  }
  if (requested < kMinWidth) {
    return kMinWidth;
  }
  if (requested > kMaxWidth) {
    return kMaxWidth;
  }
  return requested;
}

} // namespace

ParityEvaluator::ParityEvaluator(uint32_t trial_count, uint32_t width,
                                 uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed),
      width_(clampWidth(width)) {}

uint32_t ParityEvaluator::getWidth() const noexcept { return width_; }

uint32_t ParityEvaluator::inputCount() const { return width_; }

uint32_t ParityEvaluator::outputCount() const { return 1U; }

void ParityEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                      std::vector<uint32_t>& outputs,
                                      uint32_t /*trial_id*/) {
  uint32_t accumulator = 0U;
  for (uint32_t value : inputs) {
    accumulator ^= value;
  }
  outputs[0] = accumulator;
}

} // namespace beast
