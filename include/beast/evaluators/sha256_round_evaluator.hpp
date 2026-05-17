#ifndef BEAST_EVALUATORS_SHA256_ROUND_EVALUATOR_HPP_
#define BEAST_EVALUATORS_SHA256_ROUND_EVALUATOR_HPP_

// Standard
#include <cstdint>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Sha256RoundEvaluator
 * @brief Scores a program on its ability to perform one SHA-256 compression round
 *
 * The SHA-256 compression function is a 64-iteration loop where each iteration takes the
 * current 8-word working state `(a, b, c, d, e, f, g, h)`, the per-round constant `K[i]`
 * and the per-round message-schedule word `W[i]`, and emits a new 8-word state. This
 * evaluator scores a candidate program on its ability to reproduce that single round
 * transformation. It is the smallest reusable building block of the full hash, and the
 * intended first rung of a curriculum that ultimately evolves a full SHA-256 hasher
 * (round -> message schedule -> block compression -> padded hash; see the README in
 * `examples/compose-pipelines/` for the wiring intent).
 *
 * Reference (NIST FIPS 180-4):
 * - Ch(x, y, z)   = (x AND y) XOR ((NOT x) AND z)
 * - Maj(x, y, z)  = (x AND y) XOR (x AND z) XOR (y AND z)
 * - Sigma0(x)     = ROTR(x, 2)  XOR ROTR(x, 13) XOR ROTR(x, 22)
 * - Sigma1(x)     = ROTR(x, 6)  XOR ROTR(x, 11) XOR ROTR(x, 25)
 * - T1            = h + Sigma1(e) + Ch(e, f, g) + K + W
 * - T2            = Sigma0(a) + Maj(a, b, c)
 * - new a..h      = (T1 + T2, a, b, c, d + T1, e, f, g)
 *
 * Variable layout exposed to the candidate program:
 * - Var  0..7 : input working state (a, b, c, d, e, f, g, h), each a 32-bit word stored
 *               raw in an int32_t (the bit pattern is what matters; signedness is
 *               irrelevant for the scoring path).
 * - Var  8    : round constant K (one of the 64 SHA-256 constants; configurable).
 * - Var  9    : schedule word W (random per trial).
 * - Var  10   : trial id (incremented each trial so the program can detect a new round
 *               without having to compare the literal inputs).
 * - Var 11..18: outputs a', b', c', d', e', f', g', h'. The program signals completion
 *               by writing to ALL eight of these (we wait for `hasOutputDataAvailable`
 *               on every output slot before scoring).
 *
 * Scoring strategy: pure exact-match would give the GA a brick-wall fitness function --
 * 1.0 on a perfect match, 0 on anything else -- which is exactly the cryptographic
 * property that makes SHA-256 a hash. Instead we use bit-level Hamming-distance per
 * output word, averaged across the 8 outputs and across trials:
 *   per_word_score  = 1 - popcount(observed XOR expected) / 32.0
 *   per_trial_score = mean(per_word_score for w in outputs)
 *   final_score     = mean(per_trial_score for t in trials)
 * A program that emits random noise scores around 0.5 per word (half the bits right by
 * chance); a program that gets a few words exactly right but botches others lands
 * partway up the slope. This gives the GA a monotonic gradient to climb toward the
 * correct transformation.
 *
 * Construction
 * - `trial_count`         : how many independent (state, K, W) triples to evaluate per
 *                           call. More trials average out noise but cost more VM steps.
 *                           Default: 8 (matches the other evaluators).
 * - `round_constant_index`: which of the 64 SHA-256 round constants to use as K. Pinning
 *                           a single K narrows the search; the same program does not
 *                           have to be polymorphic across all 64 rounds. Range [0, 63].
 *                           Default: 0 (K = 0x428a2f98).
 * - `max_steps_per_trial` : VM step budget per trial. SHA-256 round arithmetic is more
 *                           involved than the Adder/Maximum tasks, so the default is
 *                           generously larger. Trials that exceed the budget score 0 for
 *                           themselves but do not abort the remaining trials.
 */
class Sha256RoundEvaluator : public Evaluator {
 public:
  Sha256RoundEvaluator(uint32_t trial_count, uint32_t round_constant_index,
                       uint32_t max_steps_per_trial);

  [[nodiscard]] double evaluate(const VmSession& session) override;

  [[nodiscard]] uint32_t getTrialCount() const noexcept;
  [[nodiscard]] uint32_t getRoundConstantIndex() const noexcept;
  [[nodiscard]] uint32_t getMaxStepsPerTrial() const noexcept;

  /**
   * @brief The 32-bit round constant K[i] currently being used.
   *
   * Exposed mainly so unit tests can sanity-check the constant lookup table without
   * having to maintain a parallel copy.
   */
  [[nodiscard]] uint32_t getRoundConstantValue() const noexcept;

 private:
  const uint32_t trial_count_;
  const uint32_t round_constant_index_;
  const uint32_t max_steps_per_trial_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_SHA256_ROUND_EVALUATOR_HPP_
