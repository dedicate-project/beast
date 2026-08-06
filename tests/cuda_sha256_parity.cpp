// Tier 2 acceptance: CPU and GPU SHA-256 evaluator paths must agree.
//
// Tests in this file are conditionally compiled in by CMake when BEAST_ENABLE_CUDA=ON
// (the corresponding `declare_test` line is gated). At runtime they short-circuit if
// `cuda::isCudaAvailable()` returns false -- a CUDA-enabled binary still needs to be
// useful on a CI box without a GPU, so we treat "no device" as PASS (with a notice in
// the catch2 output rather than a real assertion).

// Standard
#include <chrono>
#include <cmath>
#include <vector>

// Third-party
#include <catch2/catch.hpp>

// Internal
#include <beast/cpu_virtual_machine.hpp>
#include <beast/cuda/compiler.hpp>
#include <beast/cuda/cuda_sha256_round_evaluator.hpp>
#include <beast/evaluators/sha256_round_evaluator.hpp>
#include <beast/program.hpp>
#include <beast/vm_session.hpp>

namespace {

constexpr uint32_t kVariableCount = 32;
constexpr uint32_t kStringTableCount = 4;
constexpr uint32_t kMaxStringSize = 16;

// Score `genome` on the CPU evaluator using the same configuration the GPU path
// receives. Mirrors the `evaluate` call EvaluatorPipe would otherwise make.
double scoreOnCpu(const std::vector<unsigned char>& genome,
                  const beast::Sha256RoundEvaluator& eval_config) {
  beast::VmSession session(beast::Program(genome), kVariableCount, kStringTableCount,
                           kMaxStringSize);
  beast::Sha256RoundEvaluator eval(eval_config.getTrialCount(),
                                   eval_config.getRoundConstantIndex(),
                                   eval_config.getMaxStepsPerTrial(),
                                   eval_config.getRoundConstantsMode(),
                                   eval_config.getRoundsPerTrial());
  return eval.evaluate(session);
}

// Build a representative SHA-256-flavoured program: copies the 8 state words to the
// output range, XORs in K and W, rotates one of them. Exercises copy / xor / rotate /
// the eight-word output writeback, which is the workload the device VM was tuned for.
std::vector<unsigned char> buildShaIshProgram() {
  beast::Program prog(/*space=*/1024);
  for (int32_t i = 0; i < 8; ++i) {
    prog.copyVariable(i, true, 11 + i, true);
  }
  prog.bitWiseXorTwoVariables(8, true, 11, true);
  prog.bitWiseXorTwoVariables(9, true, 15, true);
  prog.rotateVariableRight(11, true, 7);
  prog.terminate(0);
  return prog.getData();
}

} // namespace

TEST_CASE("cuda_sha256_evaluator_matches_cpu_on_hand_built_program", "[cuda]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  const auto genome_uchar = buildShaIshProgram();
  const std::vector<uint8_t> genome_u8(genome_uchar.begin(), genome_uchar.end());

  // Same configuration on both backends. Fixed-K, single-round mode keeps the test
  // deterministic; the multi-round / cycling-K modes are covered by the second test.
  const beast::Sha256RoundEvaluator config(/*trial_count=*/4, /*round_constant_index=*/0,
                                           /*max_steps_per_trial=*/2000);
  const double cpu_score = scoreOnCpu(genome_uchar, config);

  beast::cuda::CudaSha256RoundEvaluator gpu_eval(config, kVariableCount);
  const double gpu_score = gpu_eval.evaluateOne(genome_u8);

  // The score is a sum of float-precision per-trial averages; expect tight agreement
  // (the GPU does the per-trial reduction in single precision, hence a small tolerance).
  REQUIRE(std::abs(gpu_score - cpu_score) < 1e-4);
}

TEST_CASE("cuda_sha256_evaluator_matches_cpu_on_multi_round_cycling_k", "[cuda]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  const auto genome_uchar = buildShaIshProgram();
  const std::vector<uint8_t> genome_u8(genome_uchar.begin(), genome_uchar.end());

  // 3 rounds with K cycling across the constants table -- exercises the trial-plan
  // generator's mode handling and the per-trial input/expected-output buffers.
  const beast::Sha256RoundEvaluator config(
      /*trial_count=*/6, /*round_constant_index=*/2,
      /*max_steps_per_trial=*/3000,
      beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll,
      /*rounds_per_trial=*/3);
  const double cpu_score = scoreOnCpu(genome_uchar, config);

  beast::cuda::CudaSha256RoundEvaluator gpu_eval(config, kVariableCount);
  const double gpu_score = gpu_eval.evaluateOne(genome_u8);

  REQUIRE(std::abs(gpu_score - cpu_score) < 1e-4);
}

TEST_CASE("cuda_sha256_evaluator_batches_independent_genomes", "[cuda]") {
  // Sanity check: a batch of N copies of the same genome must produce N identical
  // scores; mixed batch must produce per-genome scores that match evaluating each one
  // individually. This is the GA's actual usage pattern (one batch per generation).
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping batch check");
    return;
  }
  const auto genome_a_uchar = buildShaIshProgram();
  const std::vector<uint8_t> genome_a(genome_a_uchar.begin(), genome_a_uchar.end());

  // A second program: same as A but skips the rotate -- should score differently.
  beast::Program prog_b(/*space=*/1024);
  for (int32_t i = 0; i < 8; ++i) {
    prog_b.copyVariable(i, true, 11 + i, true);
  }
  prog_b.terminate(0);
  const auto genome_b_uchar = prog_b.getData();
  const std::vector<uint8_t> genome_b(genome_b_uchar.begin(), genome_b_uchar.end());

  const beast::Sha256RoundEvaluator config(/*trial_count=*/4, /*round_constant_index=*/0,
                                           /*max_steps_per_trial=*/2000);
  beast::cuda::CudaSha256RoundEvaluator gpu_eval(config, kVariableCount);

  const double single_a = gpu_eval.evaluateOne(genome_a);
  const double single_b = gpu_eval.evaluateOne(genome_b);

  const std::vector<std::vector<uint8_t>> batch{genome_a, genome_b, genome_a, genome_b};
  const auto batch_scores = gpu_eval.evaluate(batch, nullptr);
  REQUIRE(batch_scores.size() == batch.size());
  REQUIRE(std::abs(batch_scores[0] - single_a) < 1e-6);
  REQUIRE(std::abs(batch_scores[1] - single_b) < 1e-6);
  REQUIRE(std::abs(batch_scores[2] - single_a) < 1e-6);
  REQUIRE(std::abs(batch_scores[3] - single_b) < 1e-6);
}

TEST_CASE("cuda_sha256_evaluator_throughput_benchmark", "[cuda][benchmark]") {
  // Diagnostic benchmark, not an assertion of CPU/GPU speedup ratio. The numbers
  // get printed via WARN so the human running tests on real hardware can eyeball
  // throughput and confirm the GPU path is wired up end-to-end.
  //
  // What this test does NOT prove: that the GPU is faster than CPU at typical GA
  // population sizes. It usually isn't, for two architectural reasons:
  //   1. BEAST populations are 32-128 -- which is `population / SM_count` < 1 on
  //      modern GPUs, so we leave most SMs idle.
  //   2. The device VM is a 40-way switch; SIMT warp divergence means a warp of 32
  //      threads pays for ALL the branches each one takes. At population=1024
  //      throughput is observed at ~540M VM steps/sec on RTX 5880 vs ~5G on a
  //      single CPU core.
  // The Tier 2 win materialises when (a) populations are large (10K+), (b) multiple
  // pipelines share the GPU, or (c) the eval cost per genome is dominated by uniform
  // compute (e.g. fixed bit operations chained at high arithmetic intensity, where
  // divergence is minimal). Future work: warp-cooperative scheduling that runs one
  // genome per warp so memory accesses coalesce and divergence is bounded.
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping throughput benchmark");
    return;
  }
  // Build a CPU-heavy program: lots of arithmetic + bit ops that DO NOT write to
  // the output range, so neither backend can early-exit until the per-trial step
  // budget runs out. This is the workload shape where GPU acceleration matters
  // (compute-bound). A toy 8-instruction program is dominated by launch overhead
  // and won't show GPU speedup -- correctness still matters but the timing is
  // meaningless.
  beast::Program prog(/*space=*/8192);
  // Compute-bound loop body: arithmetic + bit ops that never touch the output range
  // and never terminate, so each thread runs the full per-trial step budget. This is
  // the regime where GPU acceleration pays off; toy programs are dominated by
  // launch/compile overhead and won't show speedup regardless of architecture.
  for (int reps = 0; reps < 60; ++reps) {
    prog.addConstantToVariable(20, 12345, true);
    prog.bitWiseXorTwoVariables(8, true, 20, true);
    prog.rotateVariableRight(20, true, 7);
    prog.bitWiseAndTwoVariables(9, true, 20, true);
    prog.bitShiftVariableLeft(20, true, 3);
  }
  // Loop back to start so the VM stays alive until the step budget is exhausted.
  prog.unconditionalJumpToRelativeAddress(-static_cast<int32_t>(prog.getPointer()));
  const auto genome_uchar = prog.getData();
  const std::vector<uint8_t> genome_u8(genome_uchar.begin(), genome_uchar.end());

  // Scale up to 1024 genomes -- a typical GA generation is 32-128 but we want enough
  // work in flight to actually keep the GPU busy past the launch-overhead floor. The
  // GPU evaluator amortises its constant-time costs across the batch, so the
  // crossover point where it beats single-core CPU lives somewhere in this range.
  constexpr uint32_t kPopulation = 1024;
  const beast::Sha256RoundEvaluator config(
      /*trial_count=*/8, /*round_constant_index=*/0,
      /*max_steps_per_trial=*/4000,
      beast::Sha256RoundEvaluator::RoundConstantsMode::CycleAll,
      /*rounds_per_trial=*/3);

  std::vector<std::vector<uint8_t>> batch(kPopulation, genome_u8);

  // CPU timing: sequential evaluate calls (no thread pool here so the baseline is
  // pure single-core to keep the comparison clean against a single GPU launch).
  const auto cpu_start = std::chrono::steady_clock::now();
  for (uint32_t g = 0; g < kPopulation; ++g) {
    (void)scoreOnCpu(genome_uchar, config);
  }
  const auto cpu_end = std::chrono::steady_clock::now();

  // GPU timing: one batch through CudaSha256RoundEvaluator. Run the call twice and
  // time the second so we exclude one-time cuda context init / first-launch overhead.
  beast::cuda::CudaSha256RoundEvaluator gpu_eval(config, kVariableCount);
  (void)gpu_eval.evaluate(batch, nullptr); // warmup
  const auto gpu_start = std::chrono::steady_clock::now();
  const auto gpu_scores = gpu_eval.evaluate(batch, nullptr);
  const auto gpu_end = std::chrono::steady_clock::now();
  const auto gpu_us = std::chrono::duration_cast<std::chrono::microseconds>(
                         gpu_end - gpu_start).count();
  const auto cpu_us = std::chrono::duration_cast<std::chrono::microseconds>(
                         cpu_end - cpu_start).count();

  WARN("Population " << kPopulation << ", ~" << genome_uchar.size()
                     << "-byte genomes :: CPU sequential single-core: " << cpu_us
                     << " us, GPU batched single-launch: " << gpu_us
                     << " us. See test comment for the architectural caveats on "
                        "CPU-vs-GPU comparison at typical population sizes.");

  REQUIRE(gpu_scores.size() == kPopulation);
}

TEST_CASE("cuda_sha256_compiler_handles_empty_program", "[cuda]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping empty-program check");
    return;
  }
  const beast::Sha256RoundEvaluator config(/*trial_count=*/2, /*round_constant_index=*/0,
                                           /*max_steps_per_trial=*/100);
  beast::cuda::CudaSha256RoundEvaluator gpu_eval(config, kVariableCount);

  // Empty genome and a batch with a couple of them. The host-side fast path runs and
  // returns the all-zero-output score for each trial.
  const std::vector<std::vector<uint8_t>> batch{{}, {}};
  const auto scores = gpu_eval.evaluate(batch, nullptr);
  REQUIRE(scores.size() == 2);
  REQUIRE(scores[0] == scores[1]);
  // Score is `1 - average_bits_wrong / 32` over 8 words; for random expected output
  // and zero observed output the popcount averages ~16 bits-wrong, giving ~0.5. Just
  // assert it's in a sensible band.
  REQUIRE(scores[0] >= 0.0);
  REQUIRE(scores[0] <= 1.0);
}
