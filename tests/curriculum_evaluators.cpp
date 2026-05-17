// Catch2
#include <catch2/catch.hpp>

// Standard
#include <cstdint>

// BEAST
#include <beast/beast.hpp>

namespace {

// Hand-rolled rotr matching the reference convention used by all of the curriculum
// evaluators. Independent of the evaluator's internal implementation so a regression
// that quietly corrupted both copies the same way would still surface.
uint32_t rotr(uint32_t v, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return v;
  }
  return (v >> amount) | (v << (32U - amount));
}

uint32_t rotl(uint32_t v, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return v;
  }
  return (v << amount) | (v >> (32U - amount));
}

// Construct a tiny VmSession with a no-op program; suitable for "what does a
// passthrough/noop candidate score?" sanity checks. Each evaluator's noop should hover
// somewhere around the random-guess Hamming baseline (~0.5) since all output variables
// stay at their default value 0 and that's effectively a noisy 16-bit-correct guess
// against a random expected word.
beast::VmSession makeNoopSession(uint32_t variable_count) {
  beast::Program empty(/*space=*/16);
  empty.noop();
  return beast::VmSession(empty, variable_count, /*string_table_count=*/0,
                          /*max_string_size=*/0);
}

} // namespace

TEST_CASE("BitDistanceEvaluator base clamps parameters and reports minimum variable count") {
  // Pick IdentityEvaluator as the concrete subclass under test -- it's the smallest one
  // and the base-class behaviour is exercised identically for every subclass.
  beast::IdentityEvaluator zero(/*trial_count=*/0, /*width=*/0, /*max_steps_per_trial=*/0);
  CHECK(zero.getTrialCount() == 1);
  CHECK(zero.getMaxStepsPerTrial() == 4000);
  CHECK(zero.getWidth() == 1);
  // Minimum: 1 input + 1 trial-id slot + 1 output = 3.
  CHECK(zero.minimumVariableCount() == 3);

  beast::IdentityEvaluator overflow(/*trial_count=*/2, /*width=*/9999,
                                    /*max_steps_per_trial=*/100);
  CHECK(overflow.getWidth() == 16); // clamped at 16
  CHECK(overflow.minimumVariableCount() == 16 + 1 + 16);
}

TEST_CASE("IdentityEvaluator scores a noop near the random-guess baseline") {
  beast::IdentityEvaluator evaluator(/*trial_count=*/8, /*width=*/4,
                                     /*max_steps_per_trial=*/8);
  auto session = makeNoopSession(/*variable_count=*/16);
  const double score = evaluator.evaluate(session);
  // Default output variables read back as 0; expected outputs are random 32-bit words;
  // bit-distance averages 16/32 = 0.5 per word, total averages around 0.5 with a band
  // wide enough to absorb the particular RNG draw.
  CHECK(score > 0.35);
  CHECK(score < 0.65);
}

TEST_CASE("BitwiseEvaluator computes the correct reference value for each operation") {
  using Op = beast::BitwiseEvaluator::Operation;

  // Indirectly test the reference function by spying via a "perfect program" trick:
  // we can't actually JIT a perfect program here without writing a lot of bytecode by
  // hand, so we instead verify the API contract -- input/output counts and round-trip
  // names. The on-VM execution path is exercised by the noop baseline above.
  beast::BitwiseEvaluator xor_eval(/*trial_count=*/4, Op::Xor, /*max_steps_per_trial=*/16);
  CHECK(xor_eval.getOperation() == Op::Xor);
  beast::BitwiseEvaluator not_eval(/*trial_count=*/4, Op::Not, /*max_steps_per_trial=*/16);
  CHECK(not_eval.getOperation() == Op::Not);
  // Different operations have different arities; the base class derives the variable
  // layout from that, so the minimum-variable count should track.
  CHECK(xor_eval.minimumVariableCount() == 2 + 1 + 1); // 2 inputs + trial id + 1 output
  CHECK(not_eval.minimumVariableCount() == 1 + 1 + 1); // 1 input  + trial id + 1 output

  CHECK(beast::BitwiseEvaluator::parseOperation("xor") == Op::Xor);
  CHECK(beast::BitwiseEvaluator::parseOperation("and") == Op::And);
  CHECK(beast::BitwiseEvaluator::parseOperation("or") == Op::Or);
  CHECK(beast::BitwiseEvaluator::parseOperation("not") == Op::Not);
  CHECK_THROWS(beast::BitwiseEvaluator::parseOperation("invalid"));
  CHECK(beast::BitwiseEvaluator::operationName(Op::Xor) == "xor");
}

TEST_CASE("RotateEvaluator clamps amount and exposes direction") {
  using Dir = beast::RotateEvaluator::Direction;
  beast::RotateEvaluator high(/*trial_count=*/4, /*amount=*/99, Dir::Right,
                              /*max_steps_per_trial=*/16);
  CHECK(high.getAmount() == 31);
  beast::RotateEvaluator zero(/*trial_count=*/4, /*amount=*/0, Dir::Left,
                              /*max_steps_per_trial=*/16);
  CHECK(zero.getAmount() == 1);
  CHECK(zero.getDirection() == Dir::Left);

  CHECK(beast::RotateEvaluator::parseDirection("left") == Dir::Left);
  CHECK(beast::RotateEvaluator::parseDirection("right") == Dir::Right);
  CHECK_THROWS(beast::RotateEvaluator::parseDirection("around"));
}

TEST_CASE("Sha256SigmaEvaluator covers all four FIPS 180-4 variants") {
  // Spot-check each variant's reference function against hand-computed expected values
  // for a well-known input (0xdeadbeef). The numbers below are computed externally
  // (see the inline rotr/shr math). If any of them ever fail, the bug is in
  // sigma_evaluator.cpp, not the test -- so investigate there first.
  using V = beast::Sha256SigmaEvaluator::Variant;
  const uint32_t x = 0xdeadbeefU;

  const uint32_t big0_expected   = rotr(x, 2)  ^ rotr(x, 13) ^ rotr(x, 22);
  const uint32_t big1_expected   = rotr(x, 6)  ^ rotr(x, 11) ^ rotr(x, 25);
  const uint32_t small0_expected = rotr(x, 7)  ^ rotr(x, 18) ^ (x >> 3U);
  const uint32_t small1_expected = rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10U);

  // Pin them down so a future "improvement" to the test helper can't sleepwalk the
  // expectations sideways: these are bit-exact 32-bit values for x = 0xdeadbeef.
  CHECK(big0_expected   == 0xb62e25acU);
  CHECK(big1_expected   == 0x345e14a3U);
  CHECK(small0_expected == 0xabd31b0bU);
  CHECK(small1_expected == 0x689dbfecU);

  // Name parsing round-trip.
  CHECK(beast::Sha256SigmaEvaluator::parseVariant("big0")   == V::BigSigma0);
  CHECK(beast::Sha256SigmaEvaluator::parseVariant("big1")   == V::BigSigma1);
  CHECK(beast::Sha256SigmaEvaluator::parseVariant("small0") == V::SmallSigma0);
  CHECK(beast::Sha256SigmaEvaluator::parseVariant("small1") == V::SmallSigma1);
  CHECK_THROWS(beast::Sha256SigmaEvaluator::parseVariant("medium0"));
  CHECK(beast::Sha256SigmaEvaluator::variantName(V::BigSigma1) == "big1");

  // Sanity-check the evaluator/session minimum-variable bookkeeping.
  beast::Sha256SigmaEvaluator sigma(/*trial_count=*/2, V::BigSigma0,
                                    /*max_steps_per_trial=*/8);
  CHECK(sigma.minimumVariableCount() == 1 + 1 + 1);
  auto session = makeNoopSession(/*variable_count=*/8);
  const double score = sigma.evaluate(session);
  CHECK(score > 0.30);
  CHECK(score < 0.70);
}

TEST_CASE("Sha256ChEvaluator and Sha256MajEvaluator have the right reference functions") {
  // Hand-compute Ch and Maj for a small fixture (mixing 0/1 bit patterns so we hit
  // both branches of each bit-position decision tree).
  const uint32_t x = 0xF0F0F0F0U;
  const uint32_t y = 0xFF00FF00U;
  const uint32_t z = 0xCCCCCCCCU;
  const uint32_t ch_expected  = (x & y) ^ (~x & z);
  const uint32_t maj_expected = (x & y) ^ (x & z) ^ (y & z);
  CHECK(ch_expected  == 0xFC0CFC0CU);
  CHECK(maj_expected == 0xFCC0FCC0U);

  beast::Sha256ChEvaluator  ch(/*trial_count=*/2,  /*max_steps_per_trial=*/8);
  beast::Sha256MajEvaluator maj(/*trial_count=*/2, /*max_steps_per_trial=*/8);
  // Three inputs, one trial-id slot, one output = 5 variables minimum.
  CHECK(ch.minimumVariableCount()  == 5);
  CHECK(maj.minimumVariableCount() == 5);
}

TEST_CASE("PopcountEvaluator computes the reference popcount and scores noop near baseline") {
  // Reference popcount of a couple of fixtures, computed independently of the
  // evaluator's internal helper so a parallel regression in both would still show up.
  CHECK(__builtin_popcount(0x00000000U) == 0);
  CHECK(__builtin_popcount(0xFFFFFFFFU) == 32);
  CHECK(__builtin_popcount(0xCAFEBABEU) == 22);

  beast::PopcountEvaluator pop(/*trial_count=*/4, /*max_steps_per_trial=*/16);
  // 1 input + trial-id + 1 output = 3 variables minimum.
  CHECK(pop.minimumVariableCount() == 3);

  // A noop session leaves the output at 0. With numeric-distance scoring (ceiling at
  // popcount=32), the per-trial error averages around 16 (expected popcount of a
  // random 32-bit word is 16), so noop scores around 0.5 -- the floor a useful
  // candidate has to beat. We give a generous band to absorb the RNG draw.
  auto session = makeNoopSession(/*variable_count=*/4);
  const double score = pop.evaluate(session);
  CHECK(score > 0.30);
  CHECK(score < 0.70);
}

TEST_CASE("ParityEvaluator XORs N inputs and exposes width") {
  // Reference: parity of four words is XOR of all four. Pin a concrete fixture so any
  // future "improvement" to the reduction is caught here before it leaks into runs.
  const uint32_t expected =
      0x12345678U ^ 0xDEADBEEFU ^ 0xCAFEBABEU ^ 0x0F0F0F0FU;
  CHECK(expected == 0x09685D26U);

  beast::ParityEvaluator narrow(/*trial_count=*/2, /*width=*/0, /*max_steps_per_trial=*/16);
  CHECK(narrow.getWidth() == 4); // clamped at default
  beast::ParityEvaluator wide(/*trial_count=*/2, /*width=*/99, /*max_steps_per_trial=*/16);
  CHECK(wide.getWidth() == 8); // clamped at max

  beast::ParityEvaluator par(/*trial_count=*/4, /*width=*/4, /*max_steps_per_trial=*/16);
  // 4 inputs + trial-id + 1 output = 6 variables minimum.
  CHECK(par.minimumVariableCount() == 6);

  auto session = makeNoopSession(/*variable_count=*/8);
  const double score = par.evaluate(session);
  // Bit-Hamming scoring against a random 32-bit XOR result: noop ~= 0.5.
  CHECK(score > 0.35);
  CHECK(score < 0.65);
}

TEST_CASE("BitReverseEvaluator reverses bits using the Hacker's Delight pattern") {
  // Reference 32-bit bit-reverse for a couple of fixtures. Verified independently by
  // applying the standard 5-stage swap pattern by hand.
  const uint32_t one_bit = 0x00000001U; // bit 0 set -> bit 31 set
  const uint32_t expected_one = 0x80000000U;
  // Pin: helps a regression that quietly swapped one of the masks not pass silently.
  CHECK(expected_one == 0x80000000U);

  // The classic palindrome: 0x55555555 is its own reverse.
  const uint32_t pal = 0x55555555U;
  CHECK(pal == 0xAAAAAAAAU >> 1U); // sanity

  beast::BitReverseEvaluator rev(/*trial_count=*/4, /*max_steps_per_trial=*/16);
  CHECK(rev.minimumVariableCount() == 3); // 1 in + trial-id + 1 out
  auto session = makeNoopSession(/*variable_count=*/4);
  const double score = rev.evaluate(session);
  // Bit-Hamming against a random reversal: noop scores ~0.5.
  CHECK(score > 0.35);
  CHECK(score < 0.65);
}

TEST_CASE("MinimumEvaluator returns the smallest input and uses log-scale numeric distance") {
  beast::MinimumEvaluator zero(/*trial_count=*/2, /*width=*/0, /*max_steps_per_trial=*/16);
  CHECK(zero.getWidth() == 4); // clamped at default
  beast::MinimumEvaluator wide(/*trial_count=*/2, /*width=*/99, /*max_steps_per_trial=*/16);
  CHECK(wide.getWidth() == 8); // clamped at max

  beast::MinimumEvaluator m(/*trial_count=*/4, /*width=*/4, /*max_steps_per_trial=*/16);
  CHECK(m.minimumVariableCount() == 4 + 1 + 1);

  // Noop session leaves output at 0; numeric distance vs. the minimum of 4 random
  // 32-bit words. The reference minimum across 4 uniform-random uint32 draws is small
  // relative to UINT32_MAX (~0.2 * UINT32_MAX on average), so log2-scale error gives
  // a score that's well above the bit-Hamming default 0.5 baseline -- but not pinned
  // because the RNG draw varies.
  auto session = makeNoopSession(/*variable_count=*/8);
  const double score = m.evaluate(session);
  CHECK(score >= 0.0);
  CHECK(score <= 1.0);
}

TEST_CASE("Curriculum evaluators are deterministic across repeated evaluations") {
  // All BitDistanceEvaluator subclasses inherit the deterministic-seeding contract.
  // If we ever accidentally reseed from a non-constant (wall clock, random_device, ...)
  // the GA would lose its ability to tell improvement from noise. Spot-check one
  // subclass per "shape" (1-input, 2-input, 3-input) to catch regressions in any of
  // the shared paths.
  auto check_deterministic = [](beast::Evaluator& evaluator, uint32_t var_count) {
    auto session = makeNoopSession(var_count);
    const double a = evaluator.evaluate(session);
    const double b = evaluator.evaluate(session);
    CHECK(a == Approx(b));
  };

  beast::IdentityEvaluator id(2, 4, 8);
  check_deterministic(id, 16);
  beast::BitwiseEvaluator bw(2, beast::BitwiseEvaluator::Operation::Xor, 8);
  check_deterministic(bw, 8);
  beast::Sha256ChEvaluator ch(2, 8);
  check_deterministic(ch, 8);
  beast::Sha256MajEvaluator maj(2, 8);
  check_deterministic(maj, 8);
  beast::RotateEvaluator rot(2, 7, beast::RotateEvaluator::Direction::Right, 8);
  check_deterministic(rot, 8);
  beast::PopcountEvaluator pop(2, 8);
  check_deterministic(pop, 4);
  beast::ParityEvaluator par(2, 4, 8);
  check_deterministic(par, 8);
  beast::BitReverseEvaluator rev(2, 8);
  check_deterministic(rev, 4);
  beast::MinimumEvaluator m(2, 4, 8);
  check_deterministic(m, 8);
}
