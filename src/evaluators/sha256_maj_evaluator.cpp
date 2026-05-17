#include <beast/evaluators/sha256_maj_evaluator.hpp>

// Standard
#include <cstdint>
#include <vector>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xA9047173U;

} // namespace

Sha256MajEvaluator::Sha256MajEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed) {}

uint32_t Sha256MajEvaluator::inputCount() const { return 3U; }

uint32_t Sha256MajEvaluator::outputCount() const { return 1U; }

void Sha256MajEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                         std::vector<uint32_t>& outputs,
                                         uint32_t /*trial_id*/) {
  const uint32_t x = inputs.at(0);
  const uint32_t y = inputs.at(1);
  const uint32_t z = inputs.at(2);
  outputs.at(0) = (x & y) ^ (x & z) ^ (y & z);
}

} // namespace beast
