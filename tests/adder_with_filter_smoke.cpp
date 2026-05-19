// Catch2
#include <catch2/catch.hpp>

// Standard
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

// BEAST
#include <beast/beast.hpp>

// Walks the new survivor-recycling FilterPipe example all the way through: loads the
// pipeline from its on-disk JSON, runs it for a couple of wall-clock seconds, and asserts
// that every pipe in the chain actually got to execute (i.e. nothing is wedged behind
// input-saturation or a loopback back-pressure deadlock). This is the regression guard
// for the class of "no progress at all" reports that have shown up twice now on
// loopback-shaped pipelines.
TEST_CASE("FilterPipe survivor-recycling example makes progress under wall-clock", "[slow]") {
  const std::filesystem::path src_root =
      std::filesystem::path(__FILE__).parent_path().parent_path();
  const auto path = src_root / "examples" / "compose-pipelines" / "adder-with-filter.json";
  REQUIRE(std::filesystem::exists(path));

  std::ifstream input(path);
  REQUIRE(input.is_open());
  nlohmann::json wrapped;
  input >> wrapped;

  // Aim the storage sink at a private tempfile so we don't clobber whatever the live
  // session might have at /tmp/beast-adder-promoted.json.
  const auto tmp_sink = std::filesystem::temp_directory_path() /
                        "beast-adder-with-filter-smoke.json";
  std::filesystem::remove(tmp_sink);
  wrapped["model"]["pipes"]["survivors"]["parameters"]["path"] = tmp_sink.string();

  auto pipeline = beast::PipelineManager::constructPipelineFromJson(wrapped["model"]);
  REQUIRE(pipeline != nullptr);

  pipeline->start();

  // Poll-until-success rather than sleep-once-and-hope. The slowest pipe in the chain
  // is the adder (`EvolutionPipe` running 32 individuals * 13 evals * 8 trials *
  // 800 VM steps per cycle). On a quiet box one cycle completes in well under a
  // second, but under `ctest -j8` -- which is how CI and local pre-merge runs the
  // suite -- 8 tests share the same CPU pool and this test can stretch to 10+ s
  // under contention. The old fixed 3 s sleep made the test flake-prone in exactly
  // that contended case.
  //
  // The new shape: tick every 100 ms, capture metrics, return early the moment every
  // expected pipe has executed at least once. Total wall-clock budget is generous
  // (45 s) so a heavily loaded CI host doesn't false-fail, but the common-case
  // runtime is whatever it takes for one adder cycle to complete (typically <2 s).
  static constexpr auto kPollInterval = std::chrono::milliseconds(100);
  static constexpr auto kMaxWait = std::chrono::seconds(45);
  static constexpr std::array<const char*, 8> kExpectedPipes{
      "factory", "mux", "adder", "stats", "filter", "discard", "split_pass", "survivors"};

  beast::Pipeline::PipelineMetrics metrics;
  const auto deadline = std::chrono::steady_clock::now() + kMaxWait;
  bool all_running = false;
  while (std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(kPollInterval);
    metrics = pipeline->getMetrics();
    all_running = true;
    for (const auto* name : kExpectedPipes) {
      const auto it = metrics.pipes.find(name);
      if (it == metrics.pipes.end() || it->second.execution_count <= 0.0) {
        all_running = false;
        break;
      }
    }
    if (all_running) {
      break;
    }
  }
  pipeline->stop();

  // Every pipe must have ticked. The factory and the passthroughs run far faster than the
  // adder, so even a single adder execute kicks the whole chain at least once.
  for (const auto* name : kExpectedPipes) {
    INFO("pipe = " << name);
    REQUIRE(metrics.pipes.count(name) > 0);
    REQUIRE(metrics.pipes.at(name).execution_count > 0.0);
  }
}
