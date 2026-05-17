#include <beast/evaluators/sha256_round_evaluator.hpp>

// Standard
#include <array>
#include <cstdint>
#include <random>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

namespace {

// NIST FIPS 180-4 SHA-256 round constants (first 32 bits of the fractional parts of the
// cube roots of the first 64 primes). Mirrored verbatim from the standard; do not edit.
constexpr std::array<uint32_t, 64> kSha256RoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr uint32_t kInputAOffset = 0;   // Var 0..7
constexpr uint32_t kInputK = 8;
constexpr uint32_t kInputW = 9;
constexpr uint32_t kInputTrialId = 10;
constexpr uint32_t kOutputAOffset = 11; // Var 11..18
constexpr uint32_t kStateWordCount = 8;
constexpr uint32_t kStateBitsPerWord = 32;

// Right-rotate; bit-identical to ROTR in FIPS 180-4. We're on C++17 so std::rotr/std::popcount
// aren't available; hand-rolled versions are trivial and don't carry an external dependency.
// The masking with 0x1F prevents UB from a 32-shift on a 32-bit value when amount is 0.
constexpr uint32_t rotr(uint32_t value, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return value;
  }
  return (value >> amount) | (value << (32U - amount));
}

constexpr uint32_t popcount32(uint32_t value) noexcept {
  // Classic bit-twiddle (Hacker's Delight). Three 32-bit adds + a multiply-shift; no loop.
  value = value - ((value >> 1U) & 0x55555555U);
  value = (value & 0x33333333U) + ((value >> 2U) & 0x33333333U);
  value = (value + (value >> 4U)) & 0x0F0F0F0FU;
  return (value * 0x01010101U) >> 24U;
}

constexpr uint32_t bigSigma0(uint32_t x) noexcept {
  return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

constexpr uint32_t bigSigma1(uint32_t x) noexcept {
  return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

constexpr uint32_t choose(uint32_t x, uint32_t y, uint32_t z) noexcept {
  return (x & y) ^ (~x & z);
}

constexpr uint32_t majority(uint32_t x, uint32_t y, uint32_t z) noexcept {
  return (x & y) ^ (x & z) ^ (y & z);
}

// Reference SHA-256 round. Mirrors the pseudocode in FIPS 180-4 sec. 6.2.2 step 3 with
// the substitution `T1 += K + W` baked in. Outputs are returned in the natural a..h
// order so the caller can drop them straight into the 8 output variables. Uses .at()
// throughout so clang-tidy's bounds-checked-array-index check is satisfied for the
// non-constant-expression accesses further down.
std::array<uint32_t, kStateWordCount>
referenceRound(const std::array<uint32_t, kStateWordCount>& in, uint32_t k, uint32_t w) {
  const uint32_t a = in.at(0);
  const uint32_t b = in.at(1);
  const uint32_t c = in.at(2);
  const uint32_t d = in.at(3);
  const uint32_t e = in.at(4);
  const uint32_t f = in.at(5);
  const uint32_t g = in.at(6);
  const uint32_t h = in.at(7);

  const uint32_t t1 = h + bigSigma1(e) + choose(e, f, g) + k + w;
  const uint32_t t2 = bigSigma0(a) + majority(a, b, c);

  return {
      t1 + t2, // new a
      a,       // new b
      b,       // new c
      c,       // new d
      d + t1,  // new e
      e,       // new f
      f,       // new g
      g,       // new h
  };
}

// Score a single output word: 1.0 for bit-exact match, smoothly decaying to 0.0 with
// Hamming distance. A uniformly random 32-bit guess averages 0.5 (16 bits wrong out of
// 32), giving the GA a meaningful gradient instead of the brick-wall fitness an
// exact-match scorer would impose on a cryptographic primitive.
double scoreWord(uint32_t expected, uint32_t observed) noexcept {
  const uint32_t diff = expected ^ observed;
  const uint32_t bits_wrong = popcount32(diff);
  return 1.0 - static_cast<double>(bits_wrong) / static_cast<double>(kStateBitsPerWord);
}

// Clamp the requested round constant index into [0, 63] so an out-of-range value from
// the UI / JSON doesn't run off the end of the constants table. Picking 0 as the
// fallback matches the documented default in the header.
constexpr uint32_t clampRoundIndex(uint32_t requested) noexcept {
  if (requested >= kSha256RoundConstants.size()) {
    return 0;
  }
  return requested;
}

} // namespace

Sha256RoundEvaluator::Sha256RoundEvaluator(uint32_t trial_count, uint32_t round_constant_index,
                                           uint32_t max_steps_per_trial)
    : trial_count_(trial_count == 0 ? 1 : trial_count),
      round_constant_index_(clampRoundIndex(round_constant_index)),
      max_steps_per_trial_(max_steps_per_trial == 0 ? 4000 : max_steps_per_trial) {}

double Sha256RoundEvaluator::evaluate(const VmSession& session) {
  VmSession local_session = session;
  for (uint32_t i = 0; i < kStateWordCount; ++i) {
    local_session.setVariableBehavior(static_cast<int32_t>(kInputAOffset + i),
                                      VmSession::VariableIoBehavior::Input);
    local_session.setVariableBehavior(static_cast<int32_t>(kOutputAOffset + i),
                                      VmSession::VariableIoBehavior::Output);
  }
  local_session.setVariableBehavior(static_cast<int32_t>(kInputK),
                                    VmSession::VariableIoBehavior::Input);
  local_session.setVariableBehavior(static_cast<int32_t>(kInputW),
                                    VmSession::VariableIoBehavior::Input);
  local_session.setVariableBehavior(static_cast<int32_t>(kInputTrialId),
                                    VmSession::VariableIoBehavior::Input);

  // Seed deterministically per evaluation so two evaluations of the same program get
  // the same trial sequence; otherwise a flaky randomised score would prevent the GA
  // from telling improvement apart from noise. Same idiom as the other evaluators.
  std::mt19937 rng(0xA5A5C0DEU); // NOLINT(cert-msc51-cpp,cert-msc32-c)
  std::uniform_int_distribution<uint32_t> word_dist(0U, 0xFFFFFFFFU);

  const uint32_t k = kSha256RoundConstants.at(round_constant_index_);

  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);

  double total_score = 0.0;
  try {
    for (uint32_t trial = 0; trial < trial_count_; ++trial) {
      std::array<uint32_t, kStateWordCount> in{};
      for (uint32_t i = 0; i < kStateWordCount; ++i) {
        in.at(i) = word_dist(rng);
        local_session.setVariableValue(
            static_cast<int32_t>(kInputAOffset + i), true, static_cast<int32_t>(in.at(i)));
      }
      const uint32_t w = word_dist(rng);
      local_session.setVariableValue(static_cast<int32_t>(kInputK), true,
                                     static_cast<int32_t>(k));
      local_session.setVariableValue(static_cast<int32_t>(kInputW), true,
                                     static_cast<int32_t>(w));
      local_session.setVariableValue(static_cast<int32_t>(kInputTrialId), true,
                                     static_cast<int32_t>(trial));

      const auto expected = referenceRound(in, k, w);

      // Step the VM until either: (a) every output variable has been written to (we
      // declare the trial complete), (b) the program halts, or (c) the per-trial step
      // budget is exhausted. Partial completion (some outputs written, others not) is
      // still scored using the latest value of each output variable -- typically those
      // unwritten variables hold 0, which scores against expected via popcount and so
      // contributes a fractional credit instead of a hard 0.
      uint32_t steps = 0;
      while (steps < max_steps_per_trial_) {
        if (!virtual_machine.step(local_session, false)) {
          break;
        }
        ++steps;
        bool all_outputs_ready = true;
        for (uint32_t i = 0; i < kStateWordCount; ++i) {
          if (!local_session.hasOutputDataAvailable(
                  static_cast<int32_t>(kOutputAOffset + i), true)) {
            all_outputs_ready = false;
            break;
          }
        }
        if (all_outputs_ready) {
          break;
        }
      }

      double trial_score = 0.0;
      for (uint32_t i = 0; i < kStateWordCount; ++i) {
        const auto observed = static_cast<uint32_t>(local_session.getVariableValue(
            static_cast<int32_t>(kOutputAOffset + i), true));
        trial_score += scoreWord(expected.at(i), observed);
      }
      total_score += trial_score / static_cast<double>(kStateWordCount);
    }
  } catch (...) {
    return 0.0;
  }

  return total_score / static_cast<double>(trial_count_);
}

uint32_t Sha256RoundEvaluator::getTrialCount() const noexcept { return trial_count_; }

uint32_t Sha256RoundEvaluator::getRoundConstantIndex() const noexcept {
  return round_constant_index_;
}

uint32_t Sha256RoundEvaluator::getMaxStepsPerTrial() const noexcept {
  return max_steps_per_trial_;
}

uint32_t Sha256RoundEvaluator::getRoundConstantValue() const noexcept {
  return kSha256RoundConstants.at(round_constant_index_);
}

} // namespace beast
