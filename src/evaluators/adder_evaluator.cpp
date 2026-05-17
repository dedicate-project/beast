#include <beast/evaluators/adder_evaluator.hpp>

// Standard
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

namespace {

constexpr uint32_t kInputA = 0;
constexpr uint32_t kInputB = 1;
constexpr uint32_t kInputTrialId = 2;
constexpr uint32_t kOutputSum = 3;

// Score a single trial: 1.0 for an exact match, 0.0 for a wildly wrong answer, smooth
// linear decay in between scaled by the magnitude of the target so big numbers don't
// require pixel-perfect precision (we don't want a 999/1000 to score the same as a
// 0/1000).
double scoreTrial(int32_t expected, int32_t observed) {
  const int32_t error = std::abs(expected - observed);
  const int32_t reference = std::max<int32_t>(1, std::abs(expected));
  if (error >= 2 * reference) {
    return 0.0;
  }
  return std::clamp(1.0 - static_cast<double>(error) / (2.0 * reference), 0.0, 1.0);
}

} // namespace

AdderEvaluator::AdderEvaluator(uint32_t trial_count, int32_t value_range,
                               uint32_t max_steps_per_trial)
    : trial_count_(trial_count == 0 ? 1 : trial_count),
      value_range_(value_range <= 0 ? 1 : value_range),
      max_steps_per_trial_(max_steps_per_trial == 0 ? 1000 : max_steps_per_trial) {}

double AdderEvaluator::evaluate(const VmSession& session) {
  VmSession local_session = session;
  local_session.setVariableBehavior(kInputA, VmSession::VariableIoBehavior::Input);
  local_session.setVariableBehavior(kInputB, VmSession::VariableIoBehavior::Input);
  local_session.setVariableBehavior(kInputTrialId, VmSession::VariableIoBehavior::Input);
  local_session.setVariableBehavior(kOutputSum, VmSession::VariableIoBehavior::Output);

  // Seed deterministically per evaluation so two evaluations of the same program get
  // the same trial sequence; otherwise a flaky randomised score would prevent the GA
  // from telling improvement apart from noise. NOLINT: clang-tidy nags about the
  // constant seed but determinism is the entire point here.
  std::mt19937 rng(0xC0FFEE);     // NOLINT(cert-msc51-cpp,cert-msc32-c)
  std::uniform_int_distribution<int32_t> value_dist(-value_range_, value_range_);

  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);

  double total_score = 0.0;
  try {
    for (uint32_t trial = 0; trial < trial_count_; ++trial) {
      const int32_t a = value_dist(rng);
      const int32_t b = value_dist(rng);
      const int32_t expected = a + b;
      local_session.setVariableValue(kInputA, true, a);
      local_session.setVariableValue(kInputB, true, b);
      local_session.setVariableValue(kInputTrialId, true, static_cast<int32_t>(trial));

      bool got_output = false;
      uint32_t steps = 0;
      while (steps < max_steps_per_trial_) {
        if (!virtual_machine.step(local_session, false)) {
          break;
        }
        if (local_session.hasOutputDataAvailable(kOutputSum, true)) {
          const int32_t observed = local_session.getVariableValue(kOutputSum, true);
          total_score += scoreTrial(expected, observed);
          got_output = true;
          break;
        }
        ++steps;
      }
      if (!got_output) {
        // Trial timed out; trial scores 0 but subsequent trials still run.
      }
    }
  } catch (...) {
    return 0.0;
  }

  return total_score / static_cast<double>(trial_count_);
}

uint32_t AdderEvaluator::getTrialCount() const noexcept { return trial_count_; }

int32_t AdderEvaluator::getValueRange() const noexcept { return value_range_; }

uint32_t AdderEvaluator::getMaxStepsPerTrial() const noexcept { return max_steps_per_trial_; }

} // namespace beast
