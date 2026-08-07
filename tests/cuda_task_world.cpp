// GPU acceptance for the TaskWorld evaluator: the device path must agree with the CPU path
// closely enough to drive the same selection pressure, and must produce a real gradient
// (a moving agent scores above a do-nothing agent).
//
// Compiled in only when BEAST_ENABLE_CUDA=ON. Skips (as PASS, with a notice) on hosts
// without a CUDA device so a CUDA-enabled binary is still runnable on GPU-less CI.

// Standard
#include <cstdint>
#include <vector>

// Third-party
#include <catch2/catch.hpp>

// Internal
#include <beast/cuda/cuda_task_world_evaluator.hpp>
#include <beast/evaluators/task_world_evaluator.hpp>
#include <beast/program.hpp>
#include <beast/vm_session.hpp>

namespace {

constexpr uint32_t kVariableCount = 40;

beast::TaskWorldEvaluator::Config makeConfig() {
  beast::TaskWorldEvaluator::Config config;
  config.rows = 5;
  config.cols = 5;
  config.difficulty = 0.0;
  config.num_items = 0;
  config.num_keys = 0;
  config.num_doors = 0;
  config.num_food = 4;
  config.radius = 2;
  config.max_steps = 400;
  config.worlds_per_eval = 4;
  config.starting_food = 0;
  config.seed_base = 987654321ULL;
  config.pool_modulus = 5;
  config.pool_residues = {1, 2, 3, 4};
  return config;
}

// The move-output variable index for radius 2: 25 grid cells + 6 sensors.
constexpr int kMoveOut = 5 * 5 + 6; // == 31

// A deterministic navigator: alternately drive DOWN (3) and RIGHT (1) by rewriting the move
// output every instruction (each write is consumed as one move). On an open 5x5 grid this
// closes distance on the goal, which sits far from the lexicographically-smallest start.
std::vector<uint8_t> buildNavigator() {
  beast::Program prog(/*space=*/1024);
  for (int i = 0; i < 40; ++i) {
    prog.setVariable(kMoveOut, (i % 2 == 0) ? 3 : 1, true);
  }
  prog.terminate(0);
  const auto data = prog.getData();
  return {data.begin(), data.end()};
}

double scoreOnCpu(const std::vector<uint8_t>& genome,
                  const beast::TaskWorldEvaluator::Config& config) {
  beast::TaskWorldEvaluator evaluator(config); // fresh: draw_counter starts at 0
  const std::vector<unsigned char> bytes(genome.begin(), genome.end());
  beast::VmSession session(beast::Program(bytes), kVariableCount, 0, 0);
  return evaluator.evaluate(session);
}

} // namespace

TEST_CASE("cuda_task_world_supports_config_limits", "[cuda][taskworld]") {
  auto config = makeConfig();
  REQUIRE(beast::cuda::CudaTaskWorldEvaluator::supportsConfig(config, kVariableCount));

  // Radius 4 blows the 64-register file (81 grid + 7 > 64) -> declined.
  config.radius = 4;
  REQUIRE_FALSE(beast::cuda::CudaTaskWorldEvaluator::supportsConfig(config, kVariableCount));

  // Grid larger than the 256-cell device cap -> declined.
  config = makeConfig();
  config.rows = 20;
  config.cols = 20;
  REQUIRE_FALSE(beast::cuda::CudaTaskWorldEvaluator::supportsConfig(config, kVariableCount));
}

TEST_CASE("cuda_task_world_matches_cpu_and_has_gradient", "[cuda][taskworld]") {
  const auto config = makeConfig();
  const auto navigator = buildNavigator();

  // A fresh CPU evaluator and a fresh GPU evaluator draw the SAME worlds_per_eval seeds from
  // the pool (identical config, both counters start at 0), so their averaged scores are
  // comparable directly.
  const double cpu_score = scoreOnCpu(navigator, config);

  beast::TaskWorldEvaluator gpu_config_source(config);
  beast::cuda::CudaTaskWorldEvaluator gpu(gpu_config_source, kVariableCount);

  if (!beast::cuda::CudaTaskWorldEvaluator::supportsConfig(config, kVariableCount)) {
    WARN("Configuration not supported by the device evaluator; skipping");
    return;
  }

  const double gpu_score = gpu.evaluateOne(navigator, 0);

  INFO("cpu_score=" << cpu_score << " gpu_score=" << gpu_score);
  REQUIRE(gpu_score >= 0.0);
  REQUIRE(gpu_score <= 1.0);
  REQUIRE(cpu_score >= 0.0);
  REQUIRE(cpu_score <= 1.0);

  // Statistical equivalence: the device port reimplements perception / movement / scoring,
  // so scores need not be bit-identical, but on the same worlds they must land close.
  REQUIRE(std::abs(cpu_score - gpu_score) < 0.2);

  // Gradient sanity: a mover must outscore a do-nothing program on the device, otherwise the
  // GA would have nothing to select on.
  beast::TaskWorldEvaluator gpu_config_source2(config);
  beast::cuda::CudaTaskWorldEvaluator gpu2(gpu_config_source2, kVariableCount);
  beast::Program empty(/*space=*/16);
  empty.terminate(0);
  const auto empty_data = empty.getData();
  const std::vector<uint8_t> empty_genome(empty_data.begin(), empty_data.end());
  const double empty_score = gpu2.evaluateOne(empty_genome, 0);
  INFO("empty_score=" << empty_score << " gpu_score=" << gpu_score);
  REQUIRE(gpu_score > empty_score);
}

TEST_CASE("cuda_task_world_partial_observability_matches_cpu", "[cuda][taskworld]") {
  // With the global compass disabled the kernel must pin the bearing sensors to a constant
  // exactly as the CPU membrane does; scores on the same worlds must still track.
  auto config = makeConfig();
  config.goal_compass = false;
  const auto navigator = buildNavigator();

  if (!beast::cuda::CudaTaskWorldEvaluator::supportsConfig(config, kVariableCount)) {
    WARN("Configuration not supported by the device evaluator; skipping");
    return;
  }

  const double cpu_score = scoreOnCpu(navigator, config);

  beast::TaskWorldEvaluator gpu_config_source(config);
  beast::cuda::CudaTaskWorldEvaluator gpu(gpu_config_source, kVariableCount);
  const double gpu_score = gpu.evaluateOne(navigator, 0);

  INFO("compass-off cpu_score=" << cpu_score << " gpu_score=" << gpu_score);
  REQUIRE(gpu_score >= 0.0);
  REQUIRE(gpu_score <= 1.0);
  REQUIRE(std::abs(cpu_score - gpu_score) < 0.2);
}
