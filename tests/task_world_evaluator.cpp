// Catch2
#include <catch2/catch.hpp>

// Standard
#include <set>

// BEAST
#include <beast/evaluators/task_world_evaluator.hpp>
#include <beast/program.hpp>
#include <beast/vm_session.hpp>

namespace {
beast::TaskWorldEvaluator::Config makeConfig() {
  beast::TaskWorldEvaluator::Config config;
  config.rows = 9;
  config.cols = 9;
  config.difficulty = 0.15;
  config.num_items = 1;
  config.num_keys = 0;
  config.num_doors = 0;
  config.num_food = 2;
  config.radius = 2;
  config.max_steps = 400;
  config.worlds_per_eval = 2;
  config.seed_base = 1;
  config.pool_modulus = 5;
  config.pool_residues = {1, 2, 3, 4};
  return config;
}
}  // namespace

TEST_CASE("TaskWorldEvaluator configuration round-trips") {
  beast::TaskWorldEvaluator evaluator(makeConfig());
  const auto& config = evaluator.getConfig();
  REQUIRE(config.rows == 9);
  REQUIRE(config.radius == 2);
  REQUIRE(config.goal_compass);  // On by default.
  REQUIRE(config.pool_residues == std::vector<uint32_t>{1, 2, 3, 4});

  auto partial = makeConfig();
  partial.goal_compass = false;
  beast::TaskWorldEvaluator partial_evaluator(partial);
  REQUIRE_FALSE(partial_evaluator.getConfig().goal_compass);
}

TEST_CASE("Partial-observability episodes stay bounded and deterministic") {
  auto config = makeConfig();
  config.goal_compass = false;
  beast::TaskWorldEvaluator evaluator(config);
  beast::Program empty_program(64);
  beast::VmSession session(empty_program, /*variable_count=*/64,
                           /*string_table_count=*/0, /*max_string_size=*/0);

  const auto first = evaluator.runEpisode(session, /*seed=*/11);
  const auto second = evaluator.runEpisode(session, /*seed=*/11);
  REQUIRE(first.score == Approx(second.score));
  REQUIRE(first.score >= 0.0);
  REQUIRE(first.score <= 1.0);
}

TEST_CASE("Train and verify seed pools are disjoint and reproducible") {
  auto train_config = makeConfig();
  auto verify_config = makeConfig();
  verify_config.pool_residues = {0};

  beast::TaskWorldEvaluator train(train_config);
  beast::TaskWorldEvaluator verify(verify_config);

  std::set<uint64_t> verify_seeds;
  for (uint64_t k = 0; k < 32; ++k) {
    const uint64_t seed = verify.enumeratedPoolSeed(k);
    REQUIRE(seed % 5 == 0);  // verify residue
    verify_seeds.insert(seed);
    // Enumeration is deterministic.
    REQUIRE(verify.enumeratedPoolSeed(k) == seed);
  }

  for (uint32_t i = 0; i < 200; ++i) {
    const uint64_t seed = train.drawPoolSeed();
    const uint32_t residue = static_cast<uint32_t>(seed % 5);
    REQUIRE(residue != 0);  // never a verify residue -> disjoint from the verify pool
    REQUIRE(verify_seeds.find(seed) == verify_seeds.end());
  }
}

TEST_CASE("Milestone scoring is bounded and monotone") {
  using EpisodeResult = beast::TaskWorldEvaluator::EpisodeResult;

  EpisodeResult empty;
  REQUIRE(beast::TaskWorldEvaluator::scoreEpisode(empty) == Approx(0.0));

  EpisodeResult partial;
  partial.items_total = 2;
  partial.items_collected = 1;
  partial.initial_distance = 10;
  partial.final_distance = 4;
  const double partial_score = beast::TaskWorldEvaluator::scoreEpisode(partial);
  REQUIRE(partial_score > 0.0);
  REQUIRE(partial_score < 1.0);

  EpisodeResult complete;
  complete.task_complete = true;
  complete.reached_goal = true;
  complete.items_total = 2;
  complete.items_collected = 2;
  complete.doors_total = 1;
  complete.doors_opened = 1;
  complete.initial_distance = 10;
  complete.final_distance = 0;
  complete.reference_moves = 12;
  complete.moves_used = 12;
  const double complete_score = beast::TaskWorldEvaluator::scoreEpisode(complete);
  REQUIRE(complete_score > partial_score);
  REQUIRE(complete_score <= 1.0);
  REQUIRE(complete_score == Approx(1.0));
}

TEST_CASE("TaskWorldEvaluator runs deterministically and stays bounded") {
  beast::TaskWorldEvaluator evaluator(makeConfig());
  beast::Program empty_program(64);
  beast::VmSession session(empty_program, /*variable_count=*/64,
                           /*string_table_count=*/0, /*max_string_size=*/0);

  const auto first = evaluator.runEpisode(session, /*seed=*/11);
  const auto second = evaluator.runEpisode(session, /*seed=*/11);
  REQUIRE(first.score == Approx(second.score));
  REQUIRE(first.score >= 0.0);
  REQUIRE(first.score <= 1.0);

  const double averaged = evaluator.evaluate(session);
  REQUIRE(averaged >= 0.0);
  REQUIRE(averaged <= 1.0);
}
