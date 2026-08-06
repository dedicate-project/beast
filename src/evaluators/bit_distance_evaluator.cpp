#include <beast/evaluators/bit_distance_evaluator.hpp>

// Standard
#include <cstdint>
#include <random>
#include <vector>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

namespace {

constexpr uint32_t kBitsPerWord = 32;

// Branchless popcount, Hacker's Delight style. C++17 doesn't have std::popcount; this
// avoids a dependency on a compiler intrinsic and stays portable across platforms.
constexpr uint32_t popcount32(uint32_t value) noexcept {
  value = value - ((value >> 1U) & 0x55555555U);
  value = (value & 0x33333333U) + ((value >> 2U) & 0x33333333U);
  value = (value + (value >> 4U)) & 0x0F0F0F0FU;
  return (value * 0x01010101U) >> 24U;
}

} // namespace

// Default scoring: bit-Hamming distance. 1.0 for an exact match, smoothly decaying to 0.0
// with the number of bit flips. The smooth gradient is the entire reason this class
// exists -- if we scored exact-match-or-zero, every member of the curriculum collapses
// into a needle-in-a-haystack search and the GA has no slope to climb.
//
// Subclasses override `scoreWord()` to use a different gradient (see e.g.
// `PopcountEvaluator` for the numeric-distance variant).
double BitDistanceEvaluator::scoreWord(uint32_t expected, uint32_t observed) const noexcept {
  const uint32_t bits_wrong = popcount32(expected ^ observed);
  return 1.0 - static_cast<double>(bits_wrong) / static_cast<double>(kBitsPerWord);
}

BitDistanceEvaluator::BitDistanceEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial,
                                           uint32_t rng_seed)
    : trial_count_(trial_count == 0 ? 1 : trial_count),
      max_steps_per_trial_(max_steps_per_trial == 0 ? 4000 : max_steps_per_trial),
      rng_seed_(rng_seed) {}

uint32_t BitDistanceEvaluator::getTrialCount() const noexcept { return trial_count_; }

uint32_t BitDistanceEvaluator::getMaxStepsPerTrial() const noexcept {
  return max_steps_per_trial_;
}

uint32_t BitDistanceEvaluator::minimumVariableCount() const noexcept {
  return inputCount() + 1U + outputCount();
}

double BitDistanceEvaluator::evaluate(const VmSession& session) {
  const uint32_t in_count = inputCount();
  const uint32_t out_count = outputCount();
  const uint32_t trial_id_var = in_count;
  const uint32_t out_offset = in_count + 1U;

  VmSession local_session = session;
  for (uint32_t i = 0; i < in_count; ++i) {
    local_session.setVariableBehavior(static_cast<int32_t>(i),
                                      VmSession::VariableIoBehavior::Input);
  }
  local_session.setVariableBehavior(static_cast<int32_t>(trial_id_var),
                                    VmSession::VariableIoBehavior::Input);
  for (uint32_t i = 0; i < out_count; ++i) {
    local_session.setVariableBehavior(static_cast<int32_t>(out_offset + i),
                                      VmSession::VariableIoBehavior::Output);
  }

  // Deterministic seeding so two evaluations of the same program produce the same
  // score; otherwise the GA would be unable to tell improvement from RNG noise.
  // NOLINT: clang-tidy nags about constant seeds for RNGs but that's the whole point.
  std::mt19937 rng(rng_seed_); // NOLINT(cert-msc51-cpp,cert-msc32-c)
  std::uniform_int_distribution<uint32_t> word_dist(0U, 0xFFFFFFFFU);

  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);

  std::vector<uint32_t> inputs(in_count);
  std::vector<uint32_t> expected(out_count);

  double total_score = 0.0;
  try {
    for (uint32_t trial = 0; trial < trial_count_; ++trial) {
      for (uint32_t i = 0; i < in_count; ++i) {
        inputs.at(i) = word_dist(rng);
        local_session.setVariableValue(static_cast<int32_t>(i), true,
                                       static_cast<int32_t>(inputs.at(i)));
      }
      local_session.setVariableValue(static_cast<int32_t>(trial_id_var), true,
                                     static_cast<int32_t>(trial));

      computeExpected(inputs, expected, trial);

      // Step the VM until either: (a) every output variable has been written to, in
      // which case we declare the trial complete; (b) the program halts on its own;
      // (c) the per-trial step budget is exhausted. In case (c) we still score whatever
      // happens to be in the output slots -- typically 0 if the program never reached
      // them, which scores via bit-distance and so contributes partial credit instead
      // of a hard zero.
      uint32_t steps = 0;
      while (steps < max_steps_per_trial_) {
        if (!virtual_machine.step(local_session, false)) {
          break;
        }
        ++steps;
        bool all_outputs_ready = true;
        for (uint32_t i = 0; i < out_count; ++i) {
          if (!local_session.hasOutputDataAvailable(static_cast<int32_t>(out_offset + i),
                                                    true)) {
            all_outputs_ready = false;
            break;
          }
        }
        if (all_outputs_ready) {
          break;
        }
      }

      double trial_score = 0.0;
      for (uint32_t i = 0; i < out_count; ++i) {
        const auto observed = static_cast<uint32_t>(local_session.getVariableValue(
            static_cast<int32_t>(out_offset + i), true));
        trial_score += scoreWord(expected.at(i), observed);
        // ^ Virtual dispatch into the subclass-supplied scoring function. Bit-Hamming
        // by default; subclasses with non-bit-shaped tasks (popcount, minimum, ...)
        // override to use numeric distance instead.
      }
      total_score += trial_score / static_cast<double>(out_count);
    }
  } catch (...) {
    return 0.0;
  }

  return total_score / static_cast<double>(trial_count_);
}

} // namespace beast
