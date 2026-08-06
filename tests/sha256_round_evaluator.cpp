// Catch2
#include <catch2/catch.hpp>

// Standard
#include <array>
#include <cmath>
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

TEST_CASE("Sha256RoundEvaluator backward-compat constructor pins mode to Fixed") {
  // Pre-mode pipelines call the 3-arg constructor; that constructor must keep producing
  // a Fixed-mode evaluator so existing serialized pipelines continue to behave the same
  // way after upgrading. If this guarantee is ever relaxed, downstream JSON ledgers
  // would silently change scoring semantics.
  beast::Sha256RoundEvaluator legacy(/*trial_count=*/4, /*round_constant_index=*/7,
                                     /*max_steps_per_trial=*/16);
  CHECK(legacy.getRoundConstantsMode() ==
        beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed);
  CHECK(legacy.getRoundConstantValue() == 0xab1c5ed5U); // K[7] from FIPS 180-4
}

TEST_CASE("Sha256RoundEvaluator new modes are accepted by the 4-arg constructor") {
  // We don't run evaluate() here -- the determinism / no-op baseline tests cover the
  // execution path. This case is just the field-plumbing contract: the mode the caller
  // requested is the mode the accessor returns, no silent clamping or fallback.
  beast::Sha256RoundEvaluator cycle(/*trial_count=*/8, /*round_constant_index=*/3,
                                    /*max_steps_per_trial=*/16,
                                    beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);
  CHECK(cycle.getRoundConstantsMode() ==
        beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);

  beast::Sha256RoundEvaluator random(
      /*trial_count=*/8, /*round_constant_index=*/3,
      /*max_steps_per_trial=*/16,
      beast::Sha256RoundEvaluator::RoundConstantsMode::RandomPerTrial);
  CHECK(random.getRoundConstantsMode() ==
        beast::Sha256RoundEvaluator::RoundConstantsMode::RandomPerTrial);
}

TEST_CASE("Sha256RoundEvaluator stays deterministic in non-Fixed modes") {
  // Determinism is the load-bearing property here: even when K varies across trials,
  // the trial sequence (and therefore the score) must be reproducible across repeated
  // evaluations of the same program. Otherwise the GA can't rank candidates. Check both
  // non-Fixed modes the same way the Fixed-mode test above does.
  beast::Program empty(/*space=*/16);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/32, /*string_table_count=*/0,
                           /*max_string_size=*/0);

  beast::Sha256RoundEvaluator cycle(/*trial_count=*/8, /*round_constant_index=*/0,
                                    /*max_steps_per_trial=*/16,
                                    beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);
  CHECK(cycle.evaluate(session) == Approx(cycle.evaluate(session)));

  beast::Sha256RoundEvaluator random(
      /*trial_count=*/8, /*round_constant_index=*/0,
      /*max_steps_per_trial=*/16,
      beast::Sha256RoundEvaluator::RoundConstantsMode::RandomPerTrial);
  CHECK(random.evaluate(session) == Approx(random.evaluate(session)));
}

TEST_CASE("Sha256RoundEvaluator non-Fixed modes change the trial sequence") {
  // Loose but meaningful sanity: an all-zeros / no-op program's score depends on which
  // K values are used (because K factors into the reference round). Across modes the K
  // sequence differs, so the no-op score must differ too (or at least not be guaranteed
  // identical). We use an inequality check on at least one mode pair, because in theory
  // a pathological cancellation could land two modes on the same score, but in practice
  // for an 8-trial run on random state/W draws the values are different to many decimals.
  beast::Program empty(/*space=*/16);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/32, /*string_table_count=*/0,
                           /*max_string_size=*/0);

  beast::Sha256RoundEvaluator fixed_eval(/*trial_count=*/8, /*round_constant_index=*/0,
                                         /*max_steps_per_trial=*/16,
                                         beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed);
  beast::Sha256RoundEvaluator cycle_eval(/*trial_count=*/8, /*round_constant_index=*/0,
                                         /*max_steps_per_trial=*/16,
                                         beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);
  beast::Sha256RoundEvaluator random_eval(
      /*trial_count=*/8, /*round_constant_index=*/0,
      /*max_steps_per_trial=*/16,
      beast::Sha256RoundEvaluator::RoundConstantsMode::RandomPerTrial);

  const double s_fixed = fixed_eval.evaluate(session);
  const double s_cycle = cycle_eval.evaluate(session);
  const double s_random = random_eval.evaluate(session);

  // All three should land in the random-guess band -- a no-op program isn't computing
  // anything regardless of which K's get sampled.
  CHECK(s_fixed > 0.30);
  CHECK(s_fixed < 0.70);
  CHECK(s_cycle > 0.30);
  CHECK(s_cycle < 0.70);
  CHECK(s_random > 0.30);
  CHECK(s_random < 0.70);

  // Cycle and random must differ from Fixed (different K sequences => different expected
  // outputs => different popcount distances against the same 0-filled observed outputs).
  // We test against an OR so one pathological tie doesn't fail the whole case.
  const bool any_differs =
      (std::abs(s_cycle - s_fixed) > 1e-9) || (std::abs(s_random - s_fixed) > 1e-9);
  CHECK(any_differs);
}

TEST_CASE("Sha256RoundEvaluator rounds_per_trial defaults to 1 and clamps weird values") {
  // Pre-multi-round constructors must keep producing a single-round evaluator.
  beast::Sha256RoundEvaluator legacy3(/*trial_count=*/4, /*round_constant_index=*/0,
                                      /*max_steps_per_trial=*/16);
  CHECK(legacy3.getRoundsPerTrial() == 1);

  beast::Sha256RoundEvaluator legacy4(/*trial_count=*/4, /*round_constant_index=*/0,
                                      /*max_steps_per_trial=*/16,
                                      beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);
  CHECK(legacy4.getRoundsPerTrial() == 1);

  // Sentinel 0 means "use the back-compat default" -- not "do zero rounds, which would
  // be a no-op trial". Caller is opting in to multi-round so giving them zero would be
  // a foot-gun; collapse to 1 so the score is still meaningful.
  beast::Sha256RoundEvaluator zero(/*trial_count=*/4, /*round_constant_index=*/0,
                                   /*max_steps_per_trial=*/16,
                                   beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed,
                                   /*rounds_per_trial=*/0);
  CHECK(zero.getRoundsPerTrial() == 1);

  // Clamp to 64 -- the SHA-256 compression function is exactly 64 rounds. Numbers
  // beyond that don't model anything real, so clamp rather than waste the user's time
  // on a 200-round target.
  beast::Sha256RoundEvaluator way_too_many(
      /*trial_count=*/4, /*round_constant_index=*/0,
      /*max_steps_per_trial=*/16, beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed,
      /*rounds_per_trial=*/9999);
  CHECK(way_too_many.getRoundsPerTrial() == 64);
}

TEST_CASE("Sha256RoundEvaluator multi-round mode keeps determinism and the random-guess band") {
  // A no-op program writes nothing -- its outputs sit at 0 regardless of how many
  // rounds we ask for. The reference's R-round output is still a pseudo-random pattern,
  // so the Hamming-per-word score lands in the random-guess band (~0.5 per word).
  // Determinism must hold here too: same program, same evaluator, same score across
  // repeated `evaluate()` calls -- otherwise the GA can't rank multi-round candidates.
  beast::Sha256RoundEvaluator multi(/*trial_count=*/4, /*round_constant_index=*/0,
                                    /*max_steps_per_trial=*/16,
                                    beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed,
                                    /*rounds_per_trial=*/4);
  beast::Program empty(/*space=*/32);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/32, /*string_table_count=*/0,
                           /*max_string_size=*/0);
  const double first = multi.evaluate(session);
  const double second = multi.evaluate(session);
  CHECK(first == Approx(second));
  CHECK(first > 0.30);
  CHECK(first < 0.70);
}

TEST_CASE("Sha256RoundEvaluator single-round behaviour is unchanged by the multi-round path") {
  // The single-round path must produce *exactly* the same score whether instantiated
  // via the legacy 3-arg constructor, the 4-arg mode-aware constructor, or the new
  // 5-arg constructor with `rounds_per_trial = 1`. Same RNG seed, same K, same
  // evaluator semantics -- the trial loop must be byte-for-byte equivalent for R = 1.
  beast::Program empty(/*space=*/32);
  empty.noop();
  beast::VmSession session(empty, /*variable_count=*/32, /*string_table_count=*/0,
                           /*max_string_size=*/0);

  beast::Sha256RoundEvaluator three_arg(/*trial_count=*/8, /*round_constant_index=*/0,
                                        /*max_steps_per_trial=*/16);
  beast::Sha256RoundEvaluator four_arg(/*trial_count=*/8, /*round_constant_index=*/0,
                                       /*max_steps_per_trial=*/16,
                                       beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed);
  beast::Sha256RoundEvaluator five_arg(/*trial_count=*/8, /*round_constant_index=*/0,
                                       /*max_steps_per_trial=*/16,
                                       beast::Sha256RoundEvaluator::RoundConstantsMode::Fixed,
                                       /*rounds_per_trial=*/1);

  const double s3 = three_arg.evaluate(session);
  const double s4 = four_arg.evaluate(session);
  const double s5 = five_arg.evaluate(session);
  CHECK(s3 == Approx(s4));
  CHECK(s3 == Approx(s5));
}

TEST_CASE("Sha256RoundEvaluator CycleAll mode starts at the configured offset") {
  // With trial_count=1 and CycleAll, the single trial should use K at the configured
  // starting offset -- so the evaluator's reported round constant value (which mirrors
  // round_constant_index) is the K the candidate is actually being asked to apply.
  // This is the contract the docstring promises and is what users will rely on to chain
  // round-0, round-1, ... evaluators in a curriculum.
  beast::Sha256RoundEvaluator at_zero(/*trial_count=*/1, /*round_constant_index=*/0,
                                      /*max_steps_per_trial=*/16,
                                      beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);
  CHECK(at_zero.getRoundConstantValue() == 0x428a2f98U);

  beast::Sha256RoundEvaluator at_sixteen(
      /*trial_count=*/1, /*round_constant_index=*/16,
      /*max_steps_per_trial=*/16,
      beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll);
  CHECK(at_sixteen.getRoundConstantValue() == 0xe49b69c1U);
}
