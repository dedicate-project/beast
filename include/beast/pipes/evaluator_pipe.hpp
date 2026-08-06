#ifndef BEAST_PIPES_EVALUATOR_PIPE_HPP_
#define BEAST_PIPES_EVALUATOR_PIPE_HPP_

// Standard
#include <memory>
#include <string>
#include <vector>

// Internal
#include <beast/evaluators/aggregation_evaluator.hpp>
#include <beast/pipes/evolution_pipe.hpp>
#include <beast/subroutine_library.hpp>

namespace beast {

/**
 * @class EvaluatorPipe
 * @brief Evolves candidate programs according to attached evaluators
 *
 * This class represents a pipeline for evolving candidate programs using evaluators that are
 * attached to it.
 *
 * The basis for this pipe implementation is an `AggregationEvaluator` instance that any amount of
 * evaluators can be attached to (even other aggregators). Candidate programs are wrapped into a
 * proper `VmSession` object and are passed to the aggregator for scoring. The environmental
 * conditions (memory size, string table details) are passed as part of this evaluator's
 * constructor.
 *
 * The idea behind this evaluator is to abstract away the evaluation process form the pipeline
 * infrastructure. No special pipes need to be implemented for linearly evolving programs as long as
 * an evaluator combination can perform the same task using the EvaluatorPipe class.
 *
 * @author Jan Winkler
 * @date 2023-03-05
 */
class EvaluatorPipe : public EvolutionPipe {
 public:
  /**
   * @brief Declarative description of one subroutine to mount on the pipe
   *
   * Mirrors the JSON shape used in the on-disk pipeline definition. At evaluation
   * time the pipe converts each source into a fully-loaded `SubroutineEntry` by
   * reading the top-K genomes from the referenced `ProgramStorageSinkPipe` ledger
   * file. `top_k` of 1 (the default) mounts only the best genome; higher values
   * mount the next-best genomes after it, consuming additional library slots.
   *
   * Empty `ledger_path` means "skip this source on load"; useful as a placeholder
   * when the user adds a row in the UI before configuring the destination.
   */
  struct SubroutineSource {
    std::string ledger_path;           ///< JSON ledger file (ProgramStorageSink output)
    uint32_t top_k = 1;                ///< Number of survivors to mount from this ledger
    uint8_t input_arity = 0;           ///< Subroutine input arity contract
    uint8_t output_arity = 0;          ///< Subroutine output arity contract
    uint32_t max_steps_per_call = 800; ///< Per-call step cap (passed to SubroutineEntry)
  };

  /**
   * @brief Execution backend selection for the GA's per-genome evaluation loop.
   *
   * The default is `Cpu`, which preserves the historical behaviour (the
   * `ThreadPoolBatchEvaluator` wrapping `EvaluatorPipe::evaluate()`). `Gpu` explicitly
   * routes through a CUDA-backed `BatchEvaluator` when the evaluator configuration
   * is supported (see `applyBackendSelection` for the exact conditions); when those
   * conditions aren't met (no CUDA device, no GPU build, unsupported evaluator type
   * or count), `Gpu` silently falls back to CPU rather than failing the pipeline.
   * `Auto` says "use GPU when you can, otherwise CPU" -- the safest opt-in for
   * users running the same JSON on a mix of GPU and CPU-only hosts.
   *
   * The enum is always present in the API regardless of `BEAST_HAS_CUDA`. On a build
   * without CUDA support, `Gpu` and `Auto` collapse to CPU at `applyBackendSelection`
   * time -- the JSON is portable, only the backend selection changes.
   */
  enum class Backend {
    Cpu = 0,  ///< Force CPU thread-pool evaluation. Default.
    Gpu = 1,  ///< Force GPU when available; silently fall back to CPU otherwise.
    Auto = 2  ///< Prefer GPU when available and applicable; CPU otherwise.
  };

  /**
   * @brief Initializes this EvaluatorPipe instance
   *
   * @param max_candidates The input/output size (number of candidate programs) of this pipe
   * @param variable_count The variable memory size for program evaluation
   * @param string_table_count The number of allowed string table entries
   * @param max_string_size The maximum allowed length of string table entries
   */
  EvaluatorPipe(uint32_t max_candidates, size_t variable_count, size_t string_table_count,
                size_t max_string_size);

  /**
   * @brief Run one evolution cycle
   *
   * Overrides `EvolutionPipe::execute` to rebuild the subroutine library from its
   * configured ledger sources at the start of each cycle. This pulls in any new
   * survivors that an upstream `ProgramStorageSinkPipe` may have written since
   * the previous cycle -- which is the entire point of mounting subroutines off a
   * sink ledger: stage 2 always sees stage 1's most recent best work.
   */
  void execute() override;

  /**
   * @brief Attaches an evaluator to this pipe
   *
   * Programs that are evolved through this pipe are subject to scoring by any attached evaluator
   * instances. This function allows to attach arbitrary types of evaluators to it.
   *
   * @param evaluator The evaluator instance to attach to this pipe
   * @param weight The relative aggregator weight to apply to this evaluator's score
   * @param invert_logic Whether to invert this evaluator's scoring logic in the aggregator
   */
  void addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight, bool invert_logic);

  /**
   * @brief Evaluates a program and returns its score
   *
   * This function evaluates a program and returns its score based on the attached evaluators.
   *
   * @param program_data The program to evaluate
   * @return The score of the program
   */
  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override;

  /**
   * @brief Returns the size of variable memory used for evaluation
   *
   * @return The size of variable memory used for evaluation
   */
  [[nodiscard]] uint32_t getMemorySize() const;

  /**
   * @brief Returns the allowed number of string table entries during evaluation
   *
   * @return The allowed number of string table entries during evaluation
   */
  [[nodiscard]] uint32_t getStringTableSize() const;

  /**
   * @brief Returns the maximum length of any string table item
   *
   * @return The maximum length of any string table item
   */
  [[nodiscard]] uint32_t getStringTableItemLength() const;

  /**
   * @brief Returns the list of attached evaluators with their respective weights and inversion
   * status
   *
   * @return The list of attached evaluators with their respective weights and inversion status
   */
  [[nodiscard]] const std::vector<AggregationEvaluator::EvaluatorDescription>&
  getEvaluators() const;

  /**
   * @brief Add a subroutine source to the pipe
   *
   * Each source contributes one or more entries to the per-evaluation
   * `SubroutineLibrary`. Sources are loaded in the order they were added; the
   * resulting `subroutine_id` indices are stable as long as the source order and
   * each source's `top_k` do not change.
   *
   * Call this before any `execute()` -- the library is rebuilt on each evaluation
   * cycle, so changes take effect immediately, but a configuration error (bad
   * `ledger_path`, arity mismatch with the on-disk bytecode) only surfaces at
   * evaluation time, not at registration time.
   */
  void addSubroutineSource(const SubroutineSource& source);

  /**
   * @brief Snapshot of the configured subroutine sources
   *
   * Exposed so the JSON serializer can round-trip the configuration without
   * having to peek at private state. Order matches `addSubroutineSource` order.
   */
  [[nodiscard]] const std::vector<SubroutineSource>& getSubroutineSources() const noexcept;

  /**
   * @brief Build a `SubroutineLibrary` by loading each configured source
   *
   * Called by `evaluate()` on every invocation. Sources with empty `ledger_path`
   * are skipped silently. Sources whose ledger file is missing, malformed, or
   * lacks the required `top_k` entries skip past the missing entries -- the
   * library is best-effort, not strict.
   *
   * Side effect: also updates the GA-side `EvolutionParameters::subroutine_arities`
   * so the factory and mutator see the same library shape as the VM.
   */
  void rebuildSubroutineLibrary();

  /**
   * @brief Snapshot of the most recently built library (may be empty)
   *
   * Exposed for diagnostics and tests; production code should rely on the library
   * being mounted on each VM session by `evaluate()`.
   */
  [[nodiscard]] std::shared_ptr<const SubroutineLibrary> getSubroutineLibrary() const noexcept;

  /**
   * @brief Configure which backend the next `applyBackendSelection()` call should
   *        install. The selection is NOT applied immediately -- the pipe loader
   *        invokes `applyBackendSelection()` after every evaluator has been attached
   *        so the backend factory can inspect the evaluator set.
   */
  void setBackend(Backend backend);

  /**
   * @brief Current backend choice. Defaults to `Backend::Cpu`.
   */
  [[nodiscard]] Backend getBackend() const noexcept;

  /**
   * @brief Install the `BatchEvaluator` that matches the configured `Backend`.
   *
   * Idempotent and cheap: re-applying the same backend swaps in a fresh
   * `BatchEvaluator` (so a config change in the underlying CPU evaluator gets
   * picked up next cycle). Call after `addEvaluator()` for every evaluator, before
   * `start()`.
   *
   * Fallback policy:
   *   - `Backend::Cpu` always uses the default thread-pool path (no-op effectively;
   *     just clears any previously injected GPU evaluator).
   *   - `Backend::Gpu` / `Backend::Auto` succeed iff: (a) the binary was built with
   *     `BEAST_ENABLE_CUDA=ON`, (b) `cuda::isCudaAvailable()` returns true, (c) the
   *     pipe has exactly one attached evaluator, and (d) that evaluator is a
   *     `Sha256RoundEvaluator` (the only GPU-backed evaluator that ships today).
   *     Anything else falls back to CPU silently; `Backend::Gpu` is "best-effort
   *     opt-in", not a hard requirement that fails the pipeline.
   */
  void applyBackendSelection();

 private:
  /**
   * @var EvaluatorPipe::variable_count_
   * @brief The size of variable memory used for evaluation
   */
  size_t variable_count_;

  /**
   * @var EvaluatorPipe::string_table_count_
   * @brief The allows number of string table entries during evaluation
   */
  size_t string_table_count_;

  /**
   * @var EvaluatorPipe::max_string_size
   * @brief The maximum length of any string table item
   */
  size_t max_string_size_;

  /**
   * @var EvaluatorPipe::evaluator_
   * @brief The base AggregationEvaluator other evaluators are attached to
   */
  AggregationEvaluator evaluator_;

  /**
   * @var EvaluatorPipe::subroutine_sources_
   * @brief Declarative subroutine source list (configured via JSON / `addSubroutineSource`)
   *
   * Kept around so the library can be rebuilt at the start of every evaluation
   * cycle (the underlying ledger files can change between cycles when an
   * upstream `ProgramStorageSinkPipe` writes new survivors).
   */
  std::vector<SubroutineSource> subroutine_sources_;

  /**
   * @var EvaluatorPipe::subroutine_library_
   * @brief Most recently built library; mounted on every VM session in `evaluate()`
   *
   * `shared_ptr<const>` so the library can be safely shared across the many
   * per-genome `VmSession`s the GA spawns each cycle without paying for copies.
   */
  std::shared_ptr<const SubroutineLibrary> subroutine_library_;

  /// Backend choice for the GA's per-genome evaluation. Defaults to CPU so existing
  /// pipeline JSON without a `backend` field behaves exactly as before.
  Backend backend_ = Backend::Cpu;
};

} // namespace beast

#endif // BEAST_PIPES_EVALUATOR_PIPE_HPP_
