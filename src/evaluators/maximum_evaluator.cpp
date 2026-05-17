#include <beast/evaluators/maximum_evaluator.hpp>

// Standard
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

namespace {

constexpr uint32_t kMinInputs = 2;
constexpr uint32_t kMaxInputs = 16;

uint32_t clampInputCount(uint32_t requested) {
  if (requested < kMinInputs) {
    return kMinInputs;
  }
  if (requested > kMaxInputs) {
    return kMaxInputs;
  }
  return requested;
}

double scoreTrial(int32_t expected_max, int32_t observed, int32_t spread) {
  if (observed == expected_max) {
    return 1.0;
  }
  const int32_t reference = std::max(1, spread);
  const int32_t error = std::abs(expected_max - observed);
  if (error >= reference) {
    return 0.0;
  }
  return std::clamp(1.0 - static_cast<double>(error) / static_cast<double>(reference), 0.0, 1.0);
}

} // namespace

MaximumEvaluator::MaximumEvaluator(uint32_t input_count, uint32_t trial_count, int32_t value_range,
                                   uint32_t max_steps_per_trial)
    : input_count_(clampInputCount(input_count)),
      trial_count_(trial_count == 0 ? 1 : trial_count),
      value_range_(value_range <= 0 ? 1 : value_range),
      max_steps_per_trial_(max_steps_per_trial == 0 ? 1000 : max_steps_per_trial) {}

double MaximumEvaluator::evaluate(const VmSession& session) {
  VmSession local_session = session;
  for (uint32_t i = 0; i < input_count_; ++i) {
    local_session.setVariableBehavior(i, VmSession::VariableIoBehavior::Input);
  }
  const uint32_t trial_id_var = input_count_;
  const uint32_t output_var = input_count_ + 1;
  local_session.setVariableBehavior(trial_id_var, VmSession::VariableIoBehavior::Input);
  local_session.setVariableBehavior(output_var, VmSession::VariableIoBehavior::Output);

  // Same determinism rationale as in AdderEvaluator: a constant seed makes the trial
  // sequence reproducible across re-evaluations.
  std::mt19937 rng(0xBADF00D);    // NOLINT(cert-msc51-cpp,cert-msc32-c)
  std::uniform_int_distribution<int32_t> value_dist(-value_range_, value_range_);

  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);

  double total_score = 0.0;
  try {
    std::vector<int32_t> values(input_count_);
    for (uint32_t trial = 0; trial < trial_count_; ++trial) {
      int32_t expected = std::numeric_limits<int32_t>::min();
      int32_t lowest = std::numeric_limits<int32_t>::max();
      for (uint32_t i = 0; i < input_count_; ++i) {
        values[i] = value_dist(rng);
        expected = std::max(expected, values[i]);
        lowest = std::min(lowest, values[i]);
        local_session.setVariableValue(i, true, values[i]);
      }
      local_session.setVariableValue(trial_id_var, true, static_cast<int32_t>(trial));
      const int32_t spread = expected - lowest;

      uint32_t steps = 0;
      while (steps < max_steps_per_trial_) {
        if (!virtual_machine.step(local_session, false)) {
          break;
        }
        if (local_session.hasOutputDataAvailable(output_var, true)) {
          const int32_t observed = local_session.getVariableValue(output_var, true);
          total_score += scoreTrial(expected, observed, spread);
          break;
        }
        ++steps;
      }
    }
  } catch (...) {
    return 0.0;
  }

  return total_score / static_cast<double>(trial_count_);
}

uint32_t MaximumEvaluator::getInputCount() const noexcept { return input_count_; }

uint32_t MaximumEvaluator::getTrialCount() const noexcept { return trial_count_; }

int32_t MaximumEvaluator::getValueRange() const noexcept { return value_range_; }

uint32_t MaximumEvaluator::getMaxStepsPerTrial() const noexcept { return max_steps_per_trial_; }

} // namespace beast
