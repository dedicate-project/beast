#ifndef BEAST_CUDA_CUDA_SHA256_ROUND_EVALUATOR_HPP_
#define BEAST_CUDA_CUDA_SHA256_ROUND_EVALUATOR_HPP_

// Standard
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// Internal
#include <beast/evaluators/sha256_round_evaluator.hpp>
#include <beast/internal/beast_ga.hpp>
#include <beast/subroutine_library.hpp>

namespace beast::cuda {

/**
 * @class CudaSha256RoundEvaluator
 * @brief GPU-backed batch evaluator that scores SHA-256 round candidates on the device.
 *
 * Pairs with `Sha256RoundEvaluator` on the CPU: same inputs, same scoring function, same
 * trial sequence under the same seed -- so scores from the two backends are within
 * floating-point tolerance for any genome whose opcodes are all GPU-supported, and
 * exactly equal for the common case (every SHA-256 evolved program uses bit ops,
 * arithmetic, and constant-target jumps -- all in the device VM's opcode set).
 *
 * This class implements the `internal::BatchEvaluator` interface so it can be installed
 * on an `EvolutionPipe` via `setBatchEvaluator()` and the GA loop automatically routes
 * the whole generation's eval through the GPU. The first call lazy-initialises the CUDA
 * context (device buffers, kernel launch parameters); subsequent calls reuse the same
 * buffers and only reupload the genome instructions.
 *
 * Lifetime contract:
 *   - Construct once per `EvolutionPipe` and inject before `start()`.
 *   - Destroy after the pipeline has been stopped; the destructor blocks on any
 *     in-flight kernel launches and frees device memory.
 *   - Not thread-safe: the GA calls `evaluate()` from one thread at a time.
 *
 * GPU resource sizing:
 *   - Compiled-instruction buffer scales with `(population * average_genome_instr_count)`.
 *     For a typical 1024-byte / ~250-instruction genome and a population of 32, this is
 *     under 128 KB -- well inside L2 on any compute-cap >= 7.0 GPU.
 *   - I/O buffers scale with `(trial_count * variable_count)`. Trivially small.
 *   - Scratch / scoring buffers scale with `(population * trial_count)`.
 *
 * For programs that the device VM doesn't fully support (an opcode it folds to NoOp,
 * a variable-target jump, etc.) the GPU score may differ from the CPU score because
 * the unsupported instructions don't run. The CPU path remains the source of truth in
 * those cases; mixed pipelines should keep their hall-of-fame and final harvest on CPU.
 */
class CudaSha256RoundEvaluator : public internal::BatchEvaluator {
 public:
  /**
   * @brief Construct.
   *
   * The configuration is copied; the source `Sha256RoundEvaluator` doesn't need to
   * outlive this object. We reuse the CPU evaluator's class for configuration to keep
   * the JSON wiring uniform -- a single `Sha256RoundEvaluator` JSON block configures
   * both backends.
   *
   * @param config A configured CPU evaluator -- we crib trial_count, k_index,
   *               max_steps_per_trial, rounds_per_trial, and round_constants_mode off it.
   * @param variable_count The total variable count the device VM allocates per thread.
   *                       Must match `EvolutionParameters::variable_count` of the owning
   *                       pipe (the SHA-256 evaluator wants at least 20).
   */
  CudaSha256RoundEvaluator(const Sha256RoundEvaluator& config, uint32_t variable_count);

  ~CudaSha256RoundEvaluator() override;

  CudaSha256RoundEvaluator(const CudaSha256RoundEvaluator&) = delete;
  CudaSha256RoundEvaluator& operator=(const CudaSha256RoundEvaluator&) = delete;
  CudaSha256RoundEvaluator(CudaSha256RoundEvaluator&&) = delete;
  CudaSha256RoundEvaluator& operator=(CudaSha256RoundEvaluator&&) = delete;

  std::vector<double> evaluate(const std::vector<std::vector<uint8_t>>& genomes,
                               const std::atomic<bool>* stop_token) override;

  /**
   * @brief Score a single genome on the GPU. Convenience for tests / one-off calls.
   *
   * Internally calls `evaluate({genome}, nullptr).at(0)`. Kept on the interface so the
   * parity tests can exercise the GPU path without standing up a full GA loop.
   */
  [[nodiscard]] double evaluateOne(const std::vector<uint8_t>& genome);

  /**
   * @brief Upload a subroutine library to the device.
   *
   * When the evaluator's GA pipeline mounts a subroutine library (via
   * `EvaluatorPipe::setSubroutineLibrary` / friends), the device-side `CallSubroutine`
   * opcode needs the library content on the GPU. Call this once after construction;
   * the library is compiled into device-friendly `Instruction` streams (one per
   * subroutine), uploaded, and held until the evaluator is destroyed or this is
   * called again with a different library.
   *
   * Passing `nullptr` (or an empty library) detaches any previously-uploaded library;
   * subsequent CallSubroutine instructions terminate the genome (matching the CPU's
   * panic-then-set-exited-abnormally policy).
   *
   * Safe to call between `evaluate()` invocations -- not safe to call concurrently
   * with one. Reuse `EvolutionPipe::stop()` if you need to swap a library mid-run.
   */
  void setSubroutineLibrary(const std::shared_ptr<beast::SubroutineLibrary>& library);

 private:
  struct Impl; ///< Holds the CUDA handles. Opaque to keep this header CUDA-free.
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Returns true if a CUDA device is present, initialised, and usable.
 *
 * Cached after the first call; safe to invoke from any thread. The check is fast (just
 * `cudaGetDeviceCount`) but we don't want every pipeline construction to repeat it.
 *
 * When this returns false, the pipeline's CUDA evaluator path silently falls back to
 * the CPU implementation -- BEAST runs on machines without GPUs.
 */
[[nodiscard]] bool isCudaAvailable() noexcept;

} // namespace beast::cuda

#endif // BEAST_CUDA_CUDA_SHA256_ROUND_EVALUATOR_HPP_
