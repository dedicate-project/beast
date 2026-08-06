#ifndef BEAST_EVOLUTION_PIPE_HPP_
#define BEAST_EVOLUTION_PIPE_HPP_

// Standard
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

// Internal
#include <beast/internal/beast_ga.hpp>
#include <beast/pipe.hpp>
#include <beast/random_program_factory.hpp>
#include <beast/vm_session.hpp>

namespace beast {

/**
 * @class EvolutionPipe
 * @brief Base class for implementing evolutionary pipes
 *
 * Evolutionary pipes are the main mechanism for fitting a set of preliminary program candidates to
 * a given task. A pipe implements the entire logic to judge the fitness of a program for the task,
 * by implementing an `evaluate` function. This class is virtual and must be subclassed.
 *
 * Genetic algorithm details
 * -------------------------
 *  - The underlying GA is BEAST's in-house implementation (`internal::BeastGA`). Per-instance
 *    RNG; no global serialisation mutex; pluggable `BatchEvaluator` so a future GPU backend
 *    drops in at one well-defined boundary.
 *  - Genomes are `std::vector<uint8_t>` (a raw byte buffer).
 *  - Mutation and crossover are operator-aware: the genome is decoded via `ProgramParser` into
 *    instruction spans, and edits happen at instruction boundaries. A configurable fraction of
 *    mutations (`EvolutionParameters::byte_mutation_share`) is still byte-level so the search
 *    can stumble onto neighbors the operator-aware mutator would never reach.
 *  - Genomes are hard-capped at `EvolutionParameters::max_genome_bytes`. This prevents the
 *    runaway "bloat" failure mode where insertions and crossover concatenation compound across
 *    generations.
 *  - Finalists stream to the output buffer as they are scored, not in one end-of-cycle burst.
 *    The hall-of-fame entry (best across all generations) is additionally stored once at the
 *    end of the cycle so transient peak scorers don't get lost when mutation pressure is high.
 *
 * @author Jan Winkler
 * @date 2023-02-04
 */
class EvolutionPipe : public Pipe {
 public:
  /**
   * @brief Tunable parameters of the in-house GA run
   *
   * `EvolutionPipe::execute` configures `internal::BeastGA` with these values. Each parameter
   * has a documented meaning and a sensible default; callers can either rely on the defaults
   * or tune per task.
   */
  struct EvolutionParameters {
    uint32_t generations = 50;          ///< Number of generations to evolve
    double crossover_probability = 0.7; ///< Probability that two parents recombine
    double mutation_probability = 0.05; ///< Per-operator mutation probability
    bool elitism = true;                ///< Carry the best individual across each generation

    /// Fraction of mutations that are byte-level (the "gamble"); the rest are operator-level.
    double byte_mutation_share = 0.2;

    /// VM environment used when minting random operators inside the mutator and as the
    /// fallback initializer.
    uint32_t variable_count = 16;
    uint32_t string_table_size = 4;
    uint32_t string_table_item_length = 16;

    /// Hard cap on genome size in bytes after mutation/crossover. We truncate at the last
    /// whole-operator boundary that fits, so the genome stays parseable.
    uint32_t max_genome_bytes = 2048;

    /// Byte budget for the random program emitted by the initializer when the input pool is
    /// empty. 0 means "use variable_count * 8" (the legacy default). Small values (8-64)
    /// shrink the search space dramatically for trivial tasks.
    uint32_t starting_program_size = 0;

    /// Per-opcode sampling weights for both initial program generation and the mutator's
    /// replace/insert operators. Empty map = uniform across all opcodes. Set entries to 0.0
    /// to forbid an opcode entirely for this stage.
    OpcodeWeights opcode_weights{};

    /// Arity table for any `CallSubroutine` instructions the GA may emit. One
    /// `(input_arity, output_arity)` entry per mounted subroutine, indexed by
    /// `subroutine_id`. Empty (the default) means "no subroutine library mounted";
    /// the factory and mutator then drop `CallSubroutine` from the opcode
    /// distribution even if the weights map says otherwise.
    ///
    /// The owner of the actual library (typically `EvaluatorPipe`) is responsible
    /// for keeping this table in sync with the bytecode it mounts on each VM
    /// session. The byte-encoder only needs arities, not bytecode, so the table is
    /// the cheapest dependency surface available to the GA-side toolchain.
    SubroutineArityTable subroutine_arities{};
  };

  /**
   * @class EvolutionPipe::EvolutionPipe
   * @brief Constructs the pipe for a maximum candidate buffer size
   *
   * Pipes have a specific initial population size that needs to be met with candidates before
   * evolution can begin. This size is specified in this constructor.
   *
   * @param max_candidates The candidate population size for this pipe
   */
  explicit EvolutionPipe(uint32_t max_candidates);

  /**
   * @class EvolutionPipe::execute
   * @brief Performs an evolution of the input candidate programs according to the Pipe's task
   *
   * Based on the `evaluate` function implemented in the concrete Pipe implementation, candidate
   * programs are scored according to their performance in these tasks. This function performs the
   * evolutionary steps required for recombination and formulation of new programs, and stores
   * programs that pass the cut-off score into the output finalist buffer.
   */
  void execute() override;

  /**
   * @class EvolutionPipe::evaluate
   * @brief Scores a candidate program according to a concrete task
   *
   * The passed in program code shall be tested for suitability for a concrete task. The better
   * the performance, the higher the score (0.0 - 1.0). Subclasses of the Pipe class need to
   * implement this function, and it is the main driver for the evolutionary step.
   */
  [[nodiscard]] virtual double evaluate(const std::vector<unsigned char>& program_data) = 0;

  /**
   * @class EvolutionPipe::setCutOffScore
   * @brief Sets the cut-off score
   *
   * Finalists that score below this score are discarded after evolution.
   */
  void setCutOffScore(double cut_off_score);

  /**
   * @brief Currently configured cut-off score (see `setCutOffScore`)
   */
  [[nodiscard]] double getCutOffScore() const noexcept;

  /**
   * @brief Replace the evolution parameter set used by the next `execute()` call
   */
  void setEvolutionParameters(const EvolutionParameters& parameters);

  /**
   * @brief Currently configured evolution parameters
   */
  [[nodiscard]] const EvolutionParameters& getEvolutionParameters() const noexcept;

  /**
   * @brief Compatibility shim for callers that want to tweak just the generation count
   *
   * Equivalent to `params.generations = generations`; preserved because the legacy
   * implementation exposed `num_generations_` as a private member with no setter and several
   * downstream demos read/wrote it through this helper in their fork.
   */
  void setNumGenerations(uint32_t generations);

  /**
   * @brief Compatibility shim for the legacy `mutation_probability_` access pattern
   */
  void setMutationProbability(double probability);

  /**
   * @brief Compatibility shim for the legacy `crossover_probability_` access pattern
   */
  void setCrossoverProbability(double probability);

  /**
   * @brief Live progress snapshot for the in-flight (or most-recent) evolve() cycle
   *
   * Surfaces enough state for a UI to render a progress bar: how many evaluator calls
   * we've made this cycle vs. an estimate of the total, how long we've been at it,
   * and what the previous cycle looked like (so the user has a baseline even before
   * the current one finishes).
   *
   * Heuristics live in `expected_evaluations_this_cycle`: for an elitism run the count is
   * `populationSize + (populationSize - 1) * generations` (initial scoring plus each
   * generation re-scoring everyone except the elite carrier). For non-elitism runs we fall
   * back to `populationSize * (generations + 1)`. This matches what BeastGA actually does
   * to within rounding and is plenty for a progress bar.
   */
  struct Progress {
    uint64_t cycle_index = 0;                       ///< Monotonically increasing, 0 = no cycle ever
    bool currently_running = false;                 ///< execute() is on the call stack right now
    uint64_t evaluations_this_cycle = 0;            ///< Evaluator callbacks fired so far
    uint64_t expected_evaluations_this_cycle = 0;   ///< Estimate (see class doc)
    double seconds_in_cycle = 0.0;                  ///< Wall-clock since the current cycle started
    double last_cycle_seconds = 0.0;                ///< Wall-clock of the most recent completed cycle
    double last_cycle_best_score = 0.0;             ///< Best score harvested from the most recent cycle
  };

  /**
   * @brief Thread-safe snapshot of the current progress state
   *
   * Cheap to call from any thread (metrics scraper) at any time. Returns a value, so the
   * caller doesn't have to hold any lock to read it.
   */
  [[nodiscard]] Progress getProgress() const noexcept;

  /**
   * @brief Override the default per-genome thread-pool evaluator with a custom one.
   *
   * Used to install GPU-backed `BatchEvaluator` implementations -- e.g.
   * `beast::cuda::CudaSha256RoundEvaluator`. The injected evaluator owns the entire
   * generation's eval; the per-genome `evaluate()` method is bypassed completely while
   * a custom evaluator is installed. Pass `nullptr` to restore the default thread-pool
   * path.
   *
   * Lifetime: the evaluator must outlive any in-flight call to `execute()`. Setting it
   * to nullptr while a cycle is in flight is undefined; only swap evaluators between
   * cycles (before `start()` or after `stop()` joins).
   */
  void setBatchEvaluator(std::unique_ptr<internal::BatchEvaluator> evaluator);

  /**
   * @brief Records one evaluator-callback invocation against the current cycle
   *
   * Public because the GA's `BatchEvaluator` wrapper is a `std::function` callback
   * supplied at construction time, not a member of `EvolutionPipe`. The implementation
   * is a single relaxed atomic increment, so unwanted external callers can at worst
   * push the progress estimate too high and nothing else. Treat as internal -- not for
   * general use.
   */
  void recordEvaluatorCall() noexcept;

 protected:
  /**
   * @class EvolutionPipe::storeFinalist
   * @brief Stores a finalist in the output buffer
   */
  void storeFinalist(const std::vector<unsigned char>& finalist, float score);

  void storeFinalist(std::vector<unsigned char>&& finalist, float score);

 private:
  /// Cut-off score below which finalists are discarded.
  double cut_off_score_ = 0.0;

  /// Currently configured GA evolution parameters.
  EvolutionParameters evolution_parameters_{};

  /// Optional override for the batch evaluator. nullptr (the default) means "build a
  /// ThreadPoolBatchEvaluator wrapping `this->evaluate()` on each `execute()` call".
  /// Non-null means BeastGA dispatches the entire generation's eval through the
  /// injected evaluator -- typically a CUDA backend like `CudaSha256RoundEvaluator`.
  std::unique_ptr<internal::BatchEvaluator> batch_evaluator_;

  /// Cycle bookkeeping. Updated by `execute()` (single-threaded per pipe) at cycle
  /// start/end, read by `getProgress()` from any thread under `progress_mutex_`.
  /// The two atomics below skip the mutex for the hot path (every evaluator call
  /// bumps `evaluations_this_cycle_`; the UI reads it 2x/sec).
  mutable std::mutex progress_mutex_;
  uint64_t cycle_index_ = 0;
  bool currently_running_ = false;
  uint64_t expected_evaluations_this_cycle_ = 0;
  std::chrono::steady_clock::time_point cycle_started_at_{};
  double last_cycle_seconds_ = 0.0;
  double last_cycle_best_score_ = 0.0;
  std::atomic<uint64_t> evaluations_this_cycle_{0};
};

} // namespace beast

#endif // BEAST_EVOLUTION_PIPE_HPP_
