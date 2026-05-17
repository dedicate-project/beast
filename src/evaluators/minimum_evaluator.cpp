#include <beast/evaluators/minimum_evaluator.hpp>

// Standard
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xA1B0CD17U;
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

// Log2 of an unsigned 64-bit value, floored, returning 0 for input 0. Used to put the
// "how wrong is the output" signal on a logarithmic scale so the gradient stays
// meaningful across the full uint32_t range -- a numeric error of 1 and one of
// 0xFFFFFFFF should score very differently, but a linear scale would collapse the
// whole interesting range into the bottom epsilon.
double log2Floor(double value) noexcept {
  if (value <= 1.0) {
    return 0.0;
  }
  return std::log2(value);
}

} // namespace

MinimumEvaluator::MinimumEvaluator(uint32_t trial_count, uint32_t width,
                                   uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed),
      width_(clampWidth(width)) {}

uint32_t MinimumEvaluator::getWidth() const noexcept { return width_; }

uint32_t MinimumEvaluator::inputCount() const { return width_; }

uint32_t MinimumEvaluator::outputCount() const { return 1U; }

void MinimumEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                       std::vector<uint32_t>& outputs,
                                       uint32_t /*trial_id*/) {
  outputs[0] = *std::min_element(inputs.begin(), inputs.end());
}

double MinimumEvaluator::scoreWord(uint32_t expected, uint32_t observed) const noexcept {
  // Log-scale numeric distance. The reference output is a uint32_t with no bounded
  // range, so a plain |a - b| / UINT32_MAX would collapse most useful gradients into
  // the bottom of the floating-point range. log2 puts "off by 4" at score 0.875 and
  // "off by 4M" at ~0.31 (and exact match at 1.0), giving the GA something to climb.
  if (expected == observed) {
    return 1.0;
  }
  const auto err = static_cast<double>(
      std::abs(static_cast<int64_t>(expected) - static_cast<int64_t>(observed)));
  // 32 = log2(UINT32_MAX + 1); divide the log-error by it to normalize to [0, 1).
  constexpr double kBits = 32.0;
  const double normalized = std::clamp(log2Floor(err + 1.0) / kBits, 0.0, 1.0);
  return 1.0 - normalized;
}

} // namespace beast
