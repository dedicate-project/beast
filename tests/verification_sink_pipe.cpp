// Catch2
#include <catch2/catch.hpp>

// Standard
#include <filesystem>
#include <fstream>
#include <vector>

#include <unistd.h>

// Third-party
#include <nlohmann/json.hpp>

// BEAST
#include <beast/evaluators/task_world_evaluator.hpp>
#include <beast/pipes/verification_sink_pipe.hpp>

namespace {
beast::TaskWorldEvaluator::Config makeVerifyConfig() {
  beast::TaskWorldEvaluator::Config config;
  config.rows = 9;
  config.cols = 9;
  config.difficulty = 0.15;
  config.num_items = 1;
  config.num_keys = 0;
  config.num_doors = 0;
  config.num_food = 2;
  config.radius = 2;
  config.max_steps = 200;
  config.seed_base = 1;
  config.pool_modulus = 5;
  config.pool_residues = {0};  // held-out residue
  return config;
}
}  // namespace

TEST_CASE("VerificationSinkPipe reports held-out generalization") {
  const std::filesystem::path report =
      std::filesystem::temp_directory_path() /
      ("beast-verify-test-" + std::to_string(::getpid()) + ".json");
  std::filesystem::remove(report);

  beast::VerificationSinkPipe pipe(/*max_candidates=*/16, report.string(), /*top_k=*/4,
                                   /*verify_worlds=*/4, /*memory_variables=*/64,
                                   /*string_table_items=*/0, /*string_table_item_length=*/0,
                                   makeVerifyConfig());

  REQUIRE(pipe.getVerifyWorlds() == 4);
  REQUIRE(pipe.getTopK() == 4);
  REQUIRE(pipe.getConfig().pool_residues == std::vector<uint32_t>{0});

  beast::Pipe::OutputItem a{std::vector<unsigned char>{}, 0.6};
  beast::Pipe::OutputItem b{std::vector<unsigned char>{1, 2, 3, 4}, 0.4};
  pipe.addInputWithScore(0, a);
  pipe.addInputWithScore(0, b);

  REQUIRE(pipe.inputsAreSaturated());
  pipe.execute();

  const auto entries = pipe.getEntries();
  REQUIRE(entries.size() == 2);
  for (const auto& entry : entries) {
    REQUIRE(entry.success_rate >= 0.0);
    REQUIRE(entry.success_rate <= 1.0);
    REQUIRE(entry.mean_score >= 0.0);
    REQUIRE(entry.mean_score <= 1.0);
  }
  // Entries are sorted by (success_rate, mean_score) descending.
  for (std::size_t i = 1; i < entries.size(); ++i) {
    const bool ordered =
        entries[i - 1].success_rate > entries[i].success_rate ||
        (entries[i - 1].success_rate == entries[i].success_rate &&
         entries[i - 1].mean_score >= entries[i].mean_score);
    REQUIRE(ordered);
  }

  REQUIRE(std::filesystem::exists(report));
  std::ifstream in(report);
  nlohmann::json doc;
  in >> doc;
  REQUIRE(doc.contains("entries"));
  REQUIRE(doc["verify_worlds"] == 4);
  REQUIRE(doc["entries"].is_array());
  REQUIRE(doc["entries"].size() == 2);
  REQUIRE(doc["entries"][0].contains("generalization_gap"));
  REQUIRE(doc["entries"][0].contains("success_rate"));

  SECTION("Duplicate programs are not double-counted") {
    pipe.addInputWithScore(0, a);
    pipe.execute();
    REQUIRE(pipe.getEntries().size() == 2);
  }

  std::filesystem::remove(report);
}
