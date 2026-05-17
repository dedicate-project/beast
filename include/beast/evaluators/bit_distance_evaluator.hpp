#ifndef BEAST_EVALUATORS_BIT_DISTANCE_EVALUATOR_HPP_
#define BEAST_EVALUATORS_BIT_DISTANCE_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class BitDistanceEvaluator
 * @brief Reusable base class for evaluators that score by bit-Hamming-distance
 *
 * The "compute N input words, transform, emit M output words" pattern shows up in every
 * member of the SHA-256 curriculum (Identity, Bitwise, Rotate, Sigma, Ch, Maj, Round)
 * and the bookkeeping is identical across all of them: declare the input/output VM
 * variables, seed an RNG deterministically, run several trials, score each output via
 * bit-distance from the reference value, average. Only the *layout* (how many inputs,
 * how many outputs) and the *reference function* (what the transformation actually is)
 * differ.
 *
 * This base class encodes the shared scaffolding. Subclasses override `inputCount()`,
 * `outputCount()`, and `computeExpected()` to define their task; everything else is
 * inherited. A subclass is typically <80 lines of straightforward code, with no
 * VM-interaction or scoring boilerplate at all.
 *
 * Variable layout (fixed across all subclasses to keep curricula composable):
 *   - Var 0 .. inputCount()-1                : inputs (set per trial)
 *   - Var inputCount()                       : trial id (incremented per trial)
 *   - Var inputCount()+1 .. inputCount()+M   : outputs (read after each trial)
 *
 * The minimum value for the host pipe's `memory_variables` (the VM session size) AND
 * for the GA's `variable_count` (the address space the mutator picks from) is therefore
 * `inputCount() + 1 + outputCount()`. Subclasses surface this via `minimumVariableCount()`
 * so the pipeline-management layer can validate / warn at configuration time.
 *
 * Scoring:
 *   per_word_score   = 1 - popcount(observed XOR expected) / 32.0
 *   per_trial_score  = mean(per_word_score across all outputs)
 *   evaluator_score  = mean(per_trial_score across all trials)
 *
 * A trial that exhausts its step budget without all outputs being written still scores
 * (using the latest value of each output variable, which is 0 if untouched); other
 * trials in the call still run.
 */
class BitDistanceEvaluator : public Evaluator {
 public:
  ~BitDistanceEvaluator() override = default;

  /**
   * @brief Runs all trials and returns the average bit-distance score in [0, 1].
   */
  [[nodiscard]] double evaluate(const VmSession& session) final;

  [[nodiscard]] uint32_t getTrialCount() const noexcept;
  [[nodiscard]] uint32_t getMaxStepsPerTrial() const noexcept;

  /**
   * @brief Smallest `variable_count` / `memory_variables` that won't truncate the I/O.
   *
   * Equal to `inputCount() + 1 + outputCount()`. The +1 is the trial-id slot at index
   * `inputCount()` that separates the input and output ranges.
   */
  [[nodiscard]] uint32_t minimumVariableCount() const noexcept;

 protected:
  /**
   * @param trial_count           Number of (random input) trials to run per evaluation.
   *                              Higher = less noise, more VM steps.
   * @param max_steps_per_trial   VM step budget per trial. Programs that don't finish
   *                              within budget still get scored on whatever they wrote.
   * @param rng_seed              Deterministic seed for the per-trial input distribution.
   *                              Subclasses pass a class-specific constant so different
   *                              evaluator types see different test vectors but the same
   *                              evaluator run twice produces identical scores.
   */
  BitDistanceEvaluator(uint32_t trial_count, uint32_t max_steps_per_trial, uint32_t rng_seed);

  /**
   * @brief How many 32-bit input words the task takes.
   */
  [[nodiscard]] virtual uint32_t inputCount() const = 0;

  /**
   * @brief How many 32-bit output words the task produces.
   */
  [[nodiscard]] virtual uint32_t outputCount() const = 0;

  /**
   * @brief Compute the reference outputs for a single trial.
   *
   * Called by the base class for each trial after random inputs have been generated.
   * Implementations should be pure functions of `inputs` and `trial_id` (the trial id
   * is provided so subclasses *can* parametrise the transformation across trials, e.g.
   * a Rotate evaluator that randomises the rotation amount per trial -- though in
   * practice every curriculum evaluator I've written so far is trial_id-independent).
   *
   * @param inputs   inputCount() random input words. Same length on every call.
   * @param outputs  Pre-sized to outputCount(). Implementations write the expected
   *                 transformation result here.
   * @param trial_id 0-indexed trial id within the current evaluate() invocation.
   */
  virtual void computeExpected(const std::vector<uint32_t>& inputs,
                               std::vector<uint32_t>& outputs, uint32_t trial_id) = 0;

  /**
   * @brief Per-output-word score in [0, 1]. 1.0 = perfect match, 0.0 = pathological.
   *
   * Default implementation: bit-Hamming distance, `1 - popcount(expected ^ observed) / 32`.
   *
   * Subclasses override this when their task has a different natural gradient. The
   * standard alternative is numeric distance, which works much better for tasks where
   * the output is a small integer (popcount in 0..32, minimum of N words, etc.) and
   * bit-flips give a misleadingly steep gradient ("you're 14 bits off" vs. "you're
   * 1 off the right number" tell very different stories about how close you are).
   */
  [[nodiscard]] virtual double scoreWord(uint32_t expected, uint32_t observed) const noexcept;

 private:
  const uint32_t trial_count_;
  const uint32_t max_steps_per_trial_;
  const uint32_t rng_seed_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_BIT_DISTANCE_EVALUATOR_HPP_
