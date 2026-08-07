#ifndef BEAST_CUDA_CUDA_TASK_WORLD_EVALUATOR_HPP_
#define BEAST_CUDA_CUDA_TASK_WORLD_EVALUATOR_HPP_

// Standard
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// Internal
#include <beast/evaluators/task_world_evaluator.hpp>
#include <beast/internal/beast_ga.hpp>

namespace beast::cuda {

/**
 * @class CudaTaskWorldEvaluator
 * @brief GPU-backed batch evaluator that runs whole TaskWorld episodes on the device.
 *
 * Unlike the SHA-256 evaluator -- a pure "set inputs, run to completion, read outputs"
 * problem -- TaskWorld is an interactive environment: the program emits a move, the world
 * mutates, and a fresh observation is fed back before the next move. There is no way to
 * bounce that loop between host and device per step without drowning in launch latency, so
 * the ENTIRE environment lives on the device: grid, line-of-sight perception, movement,
 * pickups / door unlocking, food, and the milestone score. Each CUDA thread simulates one
 * (genome, world) episode end to end and writes a single scalar fitness.
 *
 * Pairs with `TaskWorldEvaluator` on the CPU: same milestone scoring (`scoreEpisode`), same
 * host<->program membrane (perception window + scalar sensors as Input variables, one move
 * Output variable folded to `raw & 3`), and the same seed-pool draw policy. Scores are not
 * bit-identical to the CPU path -- floating-point accumulation order differs and the two
 * backends draw independent world samples -- but they are statistically equivalent, which is
 * all a fitness signal needs.
 *
 * Batch semantics: every genome in a generation is scored against the SAME `worlds_per_eval`
 * worlds (drawn fresh from the training pool each call). Sharing the sample across the
 * population is a lower-variance, fairer within-generation comparison than the CPU path's
 * per-genome draws, and it lets the kernel amortise one world upload across the whole batch.
 *
 * Device limits (validated at construction; anything larger silently declines the GPU and
 * the pipe stays on CPU): grid cells <= 256, perception window fits the 64-entry register
 * file (`(2*radius+1)^2 + 7 <= 64`, i.e. radius <= 3), items <= 8.
 *
 * Lifetime contract mirrors `CudaSha256RoundEvaluator`: construct once per pipe, inject
 * before start, destroy after stop. Not thread-safe; the GA calls `evaluate()` serially.
 */
class CudaTaskWorldEvaluator : public internal::BatchEvaluator {
 public:
  /**
   * @brief Construct from a CPU evaluator's configuration.
   *
   * @param config The TaskWorld evaluator whose Config drives world generation, the agent
   *               membrane, and the seed pool. Copied; the source need not outlive this.
   * @param variable_count Total variables the device VM allocates per thread. Must match the
   *                       owning pipe's `EvolutionParameters::variable_count` and be large
   *                       enough for the perception window plus sensors and the move output.
   */
  CudaTaskWorldEvaluator(const TaskWorldEvaluator& config, uint32_t variable_count);

  ~CudaTaskWorldEvaluator() override;

  CudaTaskWorldEvaluator(const CudaTaskWorldEvaluator&) = delete;
  CudaTaskWorldEvaluator& operator=(const CudaTaskWorldEvaluator&) = delete;
  CudaTaskWorldEvaluator(CudaTaskWorldEvaluator&&) = delete;
  CudaTaskWorldEvaluator& operator=(CudaTaskWorldEvaluator&&) = delete;

  std::vector<double> evaluate(const std::vector<std::vector<uint8_t>>& genomes,
                               const std::atomic<bool>* stop_token) override;

  /**
   * @brief Score a single genome on the GPU against one enumerated pool world.
   *
   * Convenience for tests / parity checks: uses a fixed world seed so the result is
   * reproducible without standing up a GA loop.
   */
  [[nodiscard]] double evaluateOne(const std::vector<uint8_t>& genome, uint64_t world_seed);

  /**
   * @brief Whether the configuration is within the device evaluator's supported limits.
   *
   * When false, callers should fall back to the CPU evaluator rather than construct this.
   */
  [[nodiscard]] static bool supportsConfig(const TaskWorldEvaluator::Config& config,
                                           uint32_t variable_count);

 private:
  struct Impl; ///< Holds CUDA handles + config. Opaque to keep this header CUDA-free.
  std::unique_ptr<Impl> impl_;
};

} // namespace beast::cuda

#endif // BEAST_CUDA_CUDA_TASK_WORLD_EVALUATOR_HPP_
