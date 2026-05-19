#ifndef BEAST_INTERNAL_BEAST_GA_HPP_
#define BEAST_INTERNAL_BEAST_GA_HPP_

// Standard
#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace beast::internal {

/**
 * @brief Batch evaluator -- THE GPU extension point.
 *
 * The genetic algorithm hands the evaluator a vector of byte-encoded genomes once per
 * generation and expects a vector of doubles (one score per input, aligned by index) back.
 * Everything device-specific -- thread pools, CUDA streams, batched VM execution, host /
 * device copies -- lives behind this interface. The GA itself has no idea whether scores
 * came from a CPU thread pool or a GPU kernel.
 *
 * Why "batch" rather than per-genome:
 *   - Matches the natural unit of GPU work (one launch per generation). A per-genome API
 *     would force the GPU backend to either launch N tiny kernels (terrible) or buffer
 *     internally to coalesce (defeats the purpose of the abstraction).
 *   - Lets a CPU thread-pool implementation chunk genomes across workers exactly once per
 *     batch instead of one mutex round-trip per genome.
 *   - Aligns with the way the SHA-256 and maze evaluators want to amortise per-call setup
 *     (RNG seeding, reference computation) across many trials.
 *
 * Concurrency contract:
 *   - `evaluate()` is called from a single thread (the EvolutionPipe worker). Implementations
 *     may fan out internally as they please.
 *   - By the time `evaluate()` returns, all internal threads must be quiescent -- the GA may
 *     immediately reuse / reallocate the input vector for the next generation.
 *   - The `stop_token` may be flipped concurrently with the call. Implementations should
 *     check it periodically and may short-circuit by writing sentinel zero scores for the
 *     remaining genomes; the GA discards the cycle's results when the token is set, so the
 *     specific sentinel value doesn't matter.
 */
class BatchEvaluator {
 public:
  BatchEvaluator() = default;
  virtual ~BatchEvaluator() = default;
  BatchEvaluator(const BatchEvaluator&) = delete;
  BatchEvaluator& operator=(const BatchEvaluator&) = delete;
  BatchEvaluator(BatchEvaluator&&) = delete;
  BatchEvaluator& operator=(BatchEvaluator&&) = delete;

  /**
   * @brief Score every genome in `genomes`. Returns one score per input, in input order.
   *
   * @param genomes Byte-encoded genomes to score. The caller passes by `const&`; the
   *                implementation may copy or flatten as needed.
   * @param stop_token Optional cooperative cancellation flag. May be nullptr (no cancellation).
   * @return Vector of doubles, `scores[i]` corresponds to `genomes[i]`. Must be the same
   *         size as `genomes`.
   */
  virtual std::vector<double> evaluate(
      const std::vector<std::vector<uint8_t>>& genomes,
      const std::atomic<bool>* stop_token) = 0;
};

/**
 * @brief User-supplied operators -- always CPU-side, never GPU.
 *
 * Mutation and crossover are branchy O(N) byte manipulations with no useful GPU work to
 * extract. Initialization may draw from upstream pipes or fall back to a random program
 * factory. These stay as host-side `std::function` callbacks because:
 *   1. The cost per call is negligible compared to evaluation;
 *   2. Putting them on the device would require porting `ProgramParser` + `RandomProgramFactory`
 *      to CUDA, which is a much bigger undertaking with no payoff;
 *   3. Function-pointer indirection keeps the GA agnostic to who supplies the operators
 *      (real pipe vs. test fixture).
 */
struct Operators {
  /**
   * @brief Produce one initial genome.
   *
   * Called `Config::population_size` times during `BeastGA::run()` startup. Typical
   * implementation: drain the upstream pipe input pool first, fall back to a random
   * program from `RandomProgramFactory`.
   */
  std::function<std::vector<uint8_t>()> produce_initial_genome;

  /**
   * @brief Mutate the genome in place with the given per-operator probability.
   *
   * Returns the number of mutations performed. Informational; the GA does not use the
   * return value to drive behaviour.
   */
  std::function<int(std::vector<uint8_t>& bytes, double probability)> mutate;

  /**
   * @brief Single-point crossover. Returns two children.
   *
   * Implementations are expected to splice the parents at instruction boundaries (the
   * existing operator-aware crossover does this). The GA always discards both children
   * if the crossover probability roll fails -- no special "skip crossover" mode is needed
   * here.
   */
  std::function<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>(
      const std::vector<uint8_t>& parent_a, const std::vector<uint8_t>& parent_b)>
      crossover;
};

/**
 * @brief Runtime configuration of one GA cycle.
 *
 * One `Config` per `BeastGA::run()` call. None of these knobs need to change mid-run; if
 * you want adaptive parameters, recreate the GA between runs.
 */
struct Config {
  uint32_t population_size = 24;       ///< Number of genomes in each generation
  uint32_t generations = 50;           ///< Number of generations to evolve
  double crossover_probability = 0.7;  ///< Per-pair crossover roll
  double mutation_probability = 0.05;  ///< Per-operator mutation roll
  bool elitism = true;                 ///< Carry the best individual unchanged across generations
  uint32_t tournament_size = 3;        ///< K-tournament for parent selection (2-5 is sensible)

  /// PRNG seed. **Per-instance**, eliminating the global RNG mutex that the old GAlib path
  /// required. `0` means "draw a seed from `std::random_device` at GA construction time".
  uint64_t seed = 0;
};

/**
 * @brief Callback fired once per genome whose score >= cutoff, streamed inside the loop.
 *
 * This is the "no more burst then silence" win: downstream pipes see candidates as they
 * are produced, generation by generation, rather than in one end-of-cycle burst. The
 * sink is invoked from the GA's main thread (the EvolutionPipe worker), so there is no
 * concurrency to worry about on the receiving side beyond what the pipe's existing
 * `storeOutput` already handles internally.
 *
 * The sink takes ownership of the bytes (move). If the receiver needs to keep a copy
 * elsewhere, it should clone explicitly.
 */
using FinalistSink =
    std::function<void(std::vector<uint8_t> bytes, double score)>;

/**
 * @brief Callback fired once per generation after evaluation completes.
 *
 * Used by `EvolutionPipe` to bump its progress bookkeeping (cycle-level counters,
 * "best score so far" label) without coupling the GA to the pipe's internals.
 *
 * @param generation_index Zero-based generation number that just completed. 0 = the
 *                         initial-population eval (before any step), 1..N = step N's eval.
 * @param best_score_so_far Best score the GA has ever observed in this run.
 */
using GenerationHook =
    std::function<void(uint32_t generation_index, double best_score_so_far)>;

/**
 * @class BeastGA
 * @brief Lightweight in-house genetic algorithm with per-instance RNG and pluggable evaluation.
 *
 * Replaces GAlib for `EvolutionPipe` use. Differences from the GAlib pipeline:
 *   - **Per-instance RNG**: no global serialisation mutex. Multiple `EvolutionPipe`s in the
 *     same process run their GA selection/mutation/crossover phases truly in parallel.
 *   - **`std::vector<uint8_t>` genomes**: O(N) copies, no linked-list walks. Snapshot /
 *     restore round-trips collapse from O(N^2) to O(N) per operation.
 *   - **Streaming finalists**: every cutoff-crossing genome is sinked the moment its score
 *     comes back, instead of waiting for the entire `evolve()` cycle to complete.
 *   - **Mid-generation cancellation**: each generation polls the stop token before
 *     committing eval, and the evaluator itself may short-circuit batches in flight.
 *   - **GPU-ready evaluator boundary**: `BatchEvaluator` is the single device-agnostic API
 *     a CUDA implementation needs to satisfy.
 *
 * Lifecycle:
 *   - Construct once per `EvolutionPipe::execute()`.
 *   - Call `run()` once. Reusing the same instance for multiple `run()` calls is supported
 *     but resets the population each time (the hall-of-fame is local to one `run()`).
 *
 * Thread-safety: NOT internally synchronised. One thread owns one `BeastGA`. The
 * `BatchEvaluator` may fan out internally but the GA itself is single-threaded.
 */
class BeastGA {
 public:
  /**
   * @brief Construct.
   *
   * @param config Per-cycle parameters.
   * @param operators Initialization / mutation / crossover callbacks.
   * @param evaluator Score producer. Borrowed reference; must outlive the BeastGA.
   * @param finalist_sink Optional. If unset, finalists are not streamed mid-run (caller
   *                      relies on `Result::best_*` instead).
   * @param generation_hook Optional. Called once per generation for progress reporting.
   * @param cutoff_score Genomes whose score >= this value are streamed via the sink. The
   *                     final-result hall-of-fame entry is always returned regardless of
   *                     cutoff.
   */
  BeastGA(Config config, Operators operators, BatchEvaluator& evaluator,
          FinalistSink finalist_sink, GenerationHook generation_hook,
          double cutoff_score);

  /**
   * @brief Result snapshot returned at the end of a run.
   */
  struct Result {
    std::vector<uint8_t> best_bytes;    ///< Best genome ever scored this run (empty if N/A)
    double best_score = 0.0;            ///< Its score
    uint64_t evaluations_performed = 0; ///< Total per-genome `BatchEvaluator::evaluate` count
    bool stopped_early = false;         ///< True if `stop_token` flipped during the run
  };

  /**
   * @brief Run a full cycle of `config.generations` generations.
   *
   * Cooperatively cancellable. Each generation polls `stop_token` before committing the
   * evaluation batch; the `BatchEvaluator` may itself short-circuit the batch mid-flight.
   * When the token is observed set, the run unwinds without streaming any further
   * finalists and returns `Result{stopped_early = true}`. Whatever was streamed before the
   * stop is left in the sink (caller's choice whether to discard).
   *
   * @param stop_token Optional cooperative cancellation flag. nullptr = uncancellable.
   */
  Result run(const std::atomic<bool>* stop_token);

 private:
  Config config_;
  Operators operators_;
  BatchEvaluator& evaluator_;
  FinalistSink finalist_sink_;
  GenerationHook generation_hook_;
  double cutoff_score_;

  // Per-instance RNG. The whole point of Tier 1 -- nothing else in the process touches
  // this object, so we never need a lock around its calls.
  uint64_t seed_;
};

} // namespace beast::internal

#endif // BEAST_INTERNAL_BEAST_GA_HPP_
