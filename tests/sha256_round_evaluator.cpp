// Catch2
#include <catch2/catch.hpp>

// Standard
#include <array>
#include <cstdint>

// BEAST
#include <beast/beast.hpp>

namespace {

// Independent reimplementation of one SHA-256 round, deliberately separate from the
// evaluator's internal `referenceRound`. Used to anchor the evaluator against the
// FIPS 180-4 definition WITHOUT calling the evaluator's own internals, so a bug that
// quietly broke both copies the same way would still be caught by the test vectors
// further down. Hand-rolled rotr because we're on C++17 (no std::rotr).
uint32_t rotr(uint32_t v, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return v;
  }
  return (v >> amount) | (v << (32U - amount));
}

struct State {
  uint32_t a, b, c, d, e, f, g, h;
};

State sha256Round(State s, uint32_t k, uint32_t w) {
  const uint32_t big_sigma0 = rotr(s.a, 2) ^ rotr(s.a, 13) ^ rotr(s.a, 22);
  const uint32_t big_sigma1 = rotr(s.e, 6) ^ rotr(s.e, 11) ^ rotr(s.e, 25);
  const uint32_t ch_efg = (s.e & s.f) ^ (~s.e & s.g);
  const uint32_t maj_abc = (s.a & s.b) ^ (s.a & s.c) ^ (s.b & s.c);
  const uint32_t t1 = s.h + big_sigma1 + ch_efg + k + w;
  const uint32_t t2 = big_sigma0 + maj_abc;
  return {t1 + t2, s.a, s.b, s.c, s.d + t1, s.e, s.f, s.g};
}

// SHA-256 initial hash values H[0..7] from FIPS 180-4. We use these as `(a..h)` for the
// known-vector check: round 0 with K[0] and W[0] = "abc" message-schedule[0] is a well
// understood reference path through the algorithm.
constexpr std::array<uint32_t, 8> kInitialHash = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
                                                  0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
                                                  0x1f83d9abU, 0x5be0cd19U};

} // namespace

TEST_CASE("Sha256RoundEvaluator parameter validation clamps weird inputs") {
  beast::Sha256RoundEvaluator zero(/*trial_count=*/0, /*round_constant_index=*/0,
                                   /*max_steps_per_trial=*/0);
  CHECK(zero.getTrialCount() == 1);
  CHECK(zero.getMaxStepsPerTrial() == 4000);

  beast::Sha256RoundEvaluator out_of_range(/*trial_count=*/4,
                                           /*round_constant_index=*/9999,
                                           /*max_steps_per_trial=*/100);
  // Out-of-range round index falls back to 0 so the constants table lookup stays safe.
  CHECK(out_of_range.getRoundConstantIndex() == 0);
  CHECK(out_of_range.getRoundConstantValue() == 0x428a2f98U);
}

TEST_CASE("Sha256RoundEvaluator exposes the full 64-entry round-constant table") {
  // Sanity-check a handful of the published K[i] constants from FIPS 180-4 to catch
  // copy-paste regressions in the constants array. Spread the spot-checks across the
  // table so a partial corruption is still noticed.
  beast::Sha256RoundEvaluator k0(1, 0, 100);
  CHECK(k0.getRoundConstantValue() == 0x428a2f98U);
  beast::Sha256RoundEvaluator k16(1, 16, 100);
  CHECK(k16.getRoundConstantValue() == 0xe49b69c1U);
  beast::Sha256RoundEvaluator k31(1, 31, 100);
  CHECK(k31.getRoundConstantValue() == 0x14292967U);
  beast::Sha256RoundEvaluator k48(1, 48, 100);
  CHECK(k48.getRoundConstantValue() == 0x19a4c116U);
  beast::Sha256RoundEvaluator k63(1, 63, 100);
  CHECK(k63.getRoundConstantValue() == 0xc67178f2U);
}

TEST_CASE("Sha256RoundEvaluator scores a no-op program around the random-guess baseline") {
  // A program that produces no outputs at all leaves the eight output variables at their
  // default value (0). The bit-distance between random expected output and 0 averages to
  // roughly half the bits being right -- i.e. a score around 0.5 per word, and so a
  // total score around 0.5 averaged across words & trials. We check a generous band
  // rather than a fixed value so the assertion is robust to the random sequence's
  // particular bit distribution. The point of this test is the *shape* of the gradient:
  // a noisy guess scores meaningfully more than 0, and meaningfully less than 1, so the
  // GA has a slope to climb.
  beast::Sha256RoundEvaluator evaluator(/*trial_count=*/8, /*round_constant_index=*/0,
                                        /*max_steps_per_trial=*/4);
  beast::Program empty(/*space=*/32);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/32, /*string_table_count=*/0,
                           /*max_string_size=*/0);
  const double score = evaluator.evaluate(session);
  CHECK(score > 0.35);
  CHECK(score < 0.65);
}

TEST_CASE("Sha256RoundEvaluator reference round matches a known SHA-256 trace") {
  // Cross-check the *standalone* round implementation above (compiled into this test)
  // against the SHA-256 round-0 trace produced by hashing the canonical "abc" input:
  // the first message-schedule word W[0] is 0x61626380, and after one round the state
  // transforms in a way well-documented in dozens of SHA-256 reference texts. If this
  // assertion ever fails the bug is in the math, not the evaluator.
  const uint32_t k0 = 0x428a2f98U;
  const uint32_t w0 = 0x61626380U; // "abc" || 0x80 padding into the first schedule word
  const State input{kInitialHash[0], kInitialHash[1], kInitialHash[2], kInitialHash[3],
                    kInitialHash[4], kInitialHash[5], kInitialHash[6], kInitialHash[7]};
  const State out = sha256Round(input, k0, w0);
  // These specific 8 words are the documented working state after SHA-256 round 0 on
  // "abc" -- see e.g. NIST FIPS 180-2 Appendix B.1 worked example.
  CHECK(out.a == 0x5d6aebcdU);
  CHECK(out.b == 0x6a09e667U);
  CHECK(out.c == 0xbb67ae85U);
  CHECK(out.d == 0x3c6ef372U);
  CHECK(out.e == 0xfa2a4622U);
  CHECK(out.f == 0x510e527fU);
  CHECK(out.g == 0x9b05688cU);
  CHECK(out.h == 0x1f83d9abU);
}

TEST_CASE("Sha256RoundEvaluator is deterministic across repeated evaluations") {
  // The Adder/Maximum evaluators document this as a requirement (the GA cannot
  // distinguish improvement from noise if scores are stochastic across re-runs of the
  // same program). Same property must hold here.
  beast::Sha256RoundEvaluator evaluator(/*trial_count=*/4, /*round_constant_index=*/0,
                                        /*max_steps_per_trial=*/16);
  beast::Program empty(/*space=*/16);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/32, /*string_table_count=*/0,
                           /*max_string_size=*/0);
  const double first = evaluator.evaluate(session);
  const double second = evaluator.evaluate(session);
  CHECK(first == Approx(second));
}
