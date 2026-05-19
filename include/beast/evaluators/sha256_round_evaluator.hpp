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
 * - Var 19    : `rounds_per_trial` -- the number of round applications the program is
 *               expected to perform per trial. Defaults to 1, in which case the
 *               variable is irrelevant (the original single-round behaviour). For
 *               R > 1 the program reads this as a loop trip count and is expected to
 *               apply the round transformation R times in a row using the same K, W
 *               before writing the final state to vars 11..18. Programs that ignore
 *               this variable behave as if R = 1 -- they emit one round and score
 *               against the R-round reference, which only matches for R = 1.
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
 * Memorization caveat
 * -------------------
 * The trial sequence is *deterministic across evaluations* (every program sees the same
 * RNG seed) so the GA can attribute fitness deltas to the genome rather than to noise.
 * That determinism is also the evaluator's biggest exploitable weakness: with the default
 * 8 trials and a single fixed K, the "answers" the candidate has to produce are exactly
 * 8 fixed (state, W) -> output triples + one constant K. A sufficiently fat genome
 * (>= a few hundred bytes of bytecode) can in principle encode that as a lookup table
 * and score perfectly without ever implementing the actual round transformation.
 *
 * Two knobs counter that:
 *   - Raise `trial_count`. Each extra trial adds ~544 bits of "answer" the candidate has
 *     to memorise (288 bits of input to match against + 256 bits of output to emit).
 *     At `trial_count >= 32` the lookup-table strategy starts to outgrow the typical
 *     1024-byte genome budget; at `>= 64` it's well past it.
 *   - Use `round_constants_mode != Fixed`. With `Fixed`, the candidate may hardcode the
 *     single K and ignore the K input variable. With `CycleAll` or `RandomPerTrial`, the
 *     candidate has to *read* the K input each trial because K changes per trial.
 *
 * Combine both for a search target that genuinely rewards implementing the round formula
 * over memorising a few specific bit patterns.
 *
 * Multi-round chaining
 * --------------------
 * With `rounds_per_trial == 1` (the default and the original behaviour) the candidate
 * is asked to produce exactly one SHA-256 round per trial: read `(state, K, W)`, write
 * the new state, that's the trial.
 *
 * With `rounds_per_trial >= 2` the candidate is asked to internally apply the round
 * transformation R times to a single input, in a single program invocation, and write
 * the final state. The program is invoked *once* per trial; the evaluator does NOT
 * drive the chaining, the program is expected to discover and implement the loop
 * itself (and may read the loop count from variable 19, `kInputRoundsCount`). The
 * reference applies `referenceRound` R times with the same K and W and scores the
 * candidate's final output against the reference's R-th iterate, bit-Hamming per word
 * and averaged across the 8 state words. Only the *final* state is scored;
 * intermediate states are not separately graded -- a program that nails the formula
 * but stops after R-1 rounds will score against an off-by-one expected and so come
 * out wrong, which is exactly the signal that "rounds count matters".
 *
 * Variable layout in multi-round mode (= layout in single-round mode plus var 19):
 *   - Var 19, `kInputRoundsCount`: the rounds-per-trial value R. Programs that don't
 *     read it behave as if R == 1 (the default for an unwritten input is 0; an
 *     unaware program won't loop, will emit one round at most, and so will score the
 *     same way it did pre-multi-round on a 1-round target). Programs that do read it
 *     can either compare against a hardcoded constant (cheap, R-specific) or use it
 *     as the trip count of a literal loop (general, transfers across R values).
 *   - K (var 8) and W (var 9) are the *same* K and W for all R rounds within a trial.
 *     Chaining N rounds with the same K/W is the simplest learnable target -- a
 *     curriculum stage that varies K per round (real SHA-256) needs a different
 *     variable layout (K array) and will land as its own evaluator later.
 *
 * The step budget scales with R: `effective_steps = max_steps_per_trial * R`. So
 * `max_steps_per_trial` is best understood as "steps available to compute one round",
 * not "total steps per trial". A `rounds_per_trial = 4` trial with the default 4000
 * step budget gets 16000 VM steps to do its work.
 *
 * Why this matters for the curriculum: bumping R turns the search target from
 * "reproduce one transformation" into "reproduce one transformation AND wrap it in a
 * loop". A program that's 0.875 on R=1 (state-rotation only) collapses toward 0.5
 * for R=4 because state rotation alone doesn't compose to the right R-round state.
 * Conversely, a program that genuinely implements the round formula and the loop
 * stays at 1.0 regardless of R. So R is a cheap knob to widen the fitness gap
 * between "implements the formula plus loop" and "exploits the rotation ceiling".
 *
 * Construction
 * - `trial_count`         : how many independent (initial state, K-chain, W-chain)
 *                           triples to evaluate per call. More trials average out noise
 *                           *and* increase memorisation resistance. Default: 8 (matches
 *                           the other evaluators; bump to 16-64 once your population
 *                           starts saturating).
 * - `round_constant_index`: which of the 64 SHA-256 round constants to use as K when
 *                           `round_constants_mode == Fixed`, OR the starting offset
 *                           when `mode == CycleAll` (the cycle then advances per
 *                           round-invocation, wrapping mod 64). Ignored when
 *                           `mode == RandomPerTrial`. Range [0, 63]. Default: 0
 *                           (K = 0x428a2f98).
 * - `round_constants_mode`: how K varies *across trials* (within a trial K is held
 *                           constant for all R rounds). See `RoundConstantsMode`
 *                           below. Default: `Fixed` (back-compat with pre-mode
 *                           pipelines).
 * - `max_steps_per_trial` : VM step budget for *one* round's worth of computation.
 *                           Multiplied by `rounds_per_trial` internally to get the
 *                           per-program-invocation budget, so the program has room to
 *                           loop the round transformation R times. SHA-256 round
 *                           arithmetic is more involved than the Adder/Maximum tasks,
 *                           so the default is generously larger. A trial that
 *                           exhausts the budget is still scored on whatever state
 *                           the output variables currently hold.
 * - `rounds_per_trial`    : how many SHA-256 rounds the program is expected to apply
 *                           internally per trial. 1 = original single-round scoring
 *                           (default; back-compat). >= 2 = the program is invoked
 *                           once with the initial state, K, W, and is expected to
 *                           loop the round transformation R times before writing the
 *                           final state to the output slots. The rounds-count is
 *                           exposed in variable 19 (`kInputRoundsCount`) so the
 *                           program can use it as a loop trip count. Range [1, 64].
 */
class Sha256RoundEvaluator : public Evaluator {
 public:
  /**
   * @brief Strategy for choosing the per-round constant K across trials
   *
   * - `Fixed`           : every trial uses the same K = K_table[round_constant_index].
   *                       Cheapest (memorisable). Equivalent to the pre-mode behaviour.
   * - `CycleAll`        : trial t uses K_table[(round_constant_index + t) mod 64], so
   *                       trial_count = 64 covers every SHA-256 round exactly once and
   *                       smaller trial counts walk a contiguous window starting at the
   *                       configured index. Predictable; pairs naturally with a fixed
   *                       trial_count.
   * - `RandomPerTrial`  : K is drawn from K_table by the same deterministic per-evaluation
   *                       RNG that produces the input state and W, so different trials see
   *                       different K values but the sequence is reproducible across
   *                       evaluations of the same program. `round_constant_index` is
   *                       unused in this mode.
   */
  enum class RoundConstantsMode {
    Fixed,
    CycleAll,
    RandomPerTrial,
  };

  /// Convenience constructor preserving the pre-mode (single-K, single-round) behaviour.
  /// Equivalent to calling the five-arg constructor with `RoundConstantsMode::Fixed` and
  /// `rounds_per_trial = 1`.
  Sha256RoundEvaluator(uint32_t trial_count, uint32_t round_constant_index,
                       uint32_t max_steps_per_trial);

  /// Mode-aware, single-round constructor. Equivalent to calling the five-arg constructor
  /// with `rounds_per_trial = 1`. Preserved as its own overload so call sites that already
  /// use the four-arg signature don't have to retrofit a 1.
  Sha256RoundEvaluator(uint32_t trial_count, uint32_t round_constant_index,
                       uint32_t max_steps_per_trial, RoundConstantsMode round_constants_mode);

  Sha256RoundEvaluator(uint32_t trial_count, uint32_t round_constant_index,
                       uint32_t max_steps_per_trial, RoundConstantsMode round_constants_mode,
                       uint32_t rounds_per_trial);

  [[nodiscard]] double evaluate(const VmSession& session) override;

  [[nodiscard]] uint32_t getTrialCount() const noexcept;
  [[nodiscard]] uint32_t getRoundConstantIndex() const noexcept;
  [[nodiscard]] uint32_t getMaxStepsPerTrial() const noexcept;
  [[nodiscard]] RoundConstantsMode getRoundConstantsMode() const noexcept;
  [[nodiscard]] uint32_t getRoundsPerTrial() const noexcept;

  /**
   * @brief The 32-bit round constant K[i] currently being used.
   *
   * In `Fixed` mode this is the single K every trial uses. In `CycleAll` mode it's the
   * K at the configured starting offset (the K of trial 0). In `RandomPerTrial` mode
   * the value still reflects `round_constant_index` for backwards compatibility -- the
   * actual K varies per trial inside `evaluate()` and is not exposed here.
   *
   * Exposed mainly so unit tests can sanity-check the constants table without having
   * to maintain a parallel copy.
   */
  [[nodiscard]] uint32_t getRoundConstantValue() const noexcept;

 private:
  const uint32_t trial_count_;
  const uint32_t round_constant_index_;
  const uint32_t max_steps_per_trial_;
  const RoundConstantsMode round_constants_mode_;
  const uint32_t rounds_per_trial_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_SHA256_ROUND_EVALUATOR_HPP_
