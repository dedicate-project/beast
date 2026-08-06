#include <beast/evaluators/bit_reverse_evaluator.hpp>

// Standard
#include <cstdint>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xB1B5EE5EU;

// Classic Hacker's Delight 32-bit bit-reversal: 5 swap stages of progressively
// larger groups. Branchless and constexpr-friendly so the reference function is
// trivially deterministic and as fast as the test harness needs.
constexpr uint32_t reverseBits32(uint32_t value) noexcept {
  value = ((value >> 1U) & 0x55555555U) | ((value & 0x55555555U) << 1U);
  value = ((value >> 2U) & 0x33333333U) | ((value & 0x33333333U) << 2U);
  value = ((value >> 4U) & 0x0F0F0F0FU) | ((value & 0x0F0F0F0FU) << 4U);
  value = ((value >> 8U) & 0x00FF00FFU) | ((value & 0x00FF00FFU) << 8U);
  return (value >> 16U) | (value << 16U);
}

} // namespace

BitReverseEvaluator::BitReverseEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed) {}

uint32_t BitReverseEvaluator::inputCount() const { return 1U; }

uint32_t BitReverseEvaluator::outputCount() const { return 1U; }

void BitReverseEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                          std::vector<uint32_t>& outputs,
                                          uint32_t /*trial_id*/) {
  outputs[0] = reverseBits32(inputs[0]);
}

} // namespace beast
