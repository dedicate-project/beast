#include <beast/evaluators/popcount_evaluator.hpp>

// Standard
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xF09C0117U;
constexpr uint32_t kMaxPopcount = 32U;

constexpr uint32_t popcount32(uint32_t value) noexcept {
  value = value - ((value >> 1U) & 0x55555555U);
  value = (value & 0x33333333U) + ((value >> 2U) & 0x33333333U);
  value = (value + (value >> 4U)) & 0x0F0F0F0FU;
  return (value * 0x01010101U) >> 24U;
}

} // namespace

PopcountEvaluator::PopcountEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed) {}

uint32_t PopcountEvaluator::inputCount() const { return 1U; }

uint32_t PopcountEvaluator::outputCount() const { return 1U; }

void PopcountEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                        std::vector<uint32_t>& outputs,
                                        uint32_t /*trial_id*/) {
  outputs[0] = popcount32(inputs[0]);
}

double PopcountEvaluator::scoreWord(uint32_t expected, uint32_t observed) const noexcept {
  // Numeric distance with a ceiling of kMaxPopcount (the largest representable
  // popcount of a 32-bit input). The genome may write any uint32_t to the output slot,
  // so clamp the absolute error to that ceiling before normalising; without the clamp,
  // a wild "0xCAFEBABE in the output" would give a score so negative that it'd
  // dominate the trial average and obliterate the gradient from useful neighbours.
  const auto err = static_cast<double>(
      std::abs(static_cast<int64_t>(expected) - static_cast<int64_t>(observed)));
  const double clamped = std::min(err, static_cast<double>(kMaxPopcount));
  return 1.0 - clamped / static_cast<double>(kMaxPopcount);
}

} // namespace beast
