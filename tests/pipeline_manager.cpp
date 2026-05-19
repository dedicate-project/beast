#include <catch2/catch.hpp>

// Standard
#include <filesystem>
#include <fstream>

// BEAST
#include <beast/beast.hpp>

namespace beast {

TEST_CASE("PipelineManager") {
  std::filesystem::path temp_storage_path =
      std::filesystem::temp_directory_path() / "test_pipelines";
  PipelineManager manager(temp_storage_path.string(), 100, 50);

  SECTION("Create and get pipeline") {
    const std::string pipeline_name = "Test pipeline";
    const uint32_t id = manager.createPipeline(pipeline_name);
    const PipelineManager::PipelineDescriptor& pipeline = manager.getPipelineById(id);

    REQUIRE(pipeline.id == id);
    REQUIRE(pipeline.name == pipeline_name);
    REQUIRE(pipeline.filename == "Test_pipeline.json");
  }

  SECTION("Get non-existent pipeline") {
    REQUIRE_THROWS_AS(manager.getPipelineById(100), std::invalid_argument);
  }

  SECTION("Get all pipelines") {
    const uint32_t id1 = manager.createPipeline("Test Pipeline 1");
    const uint32_t id2 = manager.createPipeline("Test Pipeline 2");

    const std::list<PipelineManager::PipelineDescriptor>& pipelines = manager.getPipelines();

    REQUIRE(pipelines.size() == 2);
    REQUIRE(pipelines.front().id == id1);
    REQUIRE(pipelines.back().id == id2);
  }

  SECTION("Load pipelines from disk") {
    const std::string pipeline_name = "Some test pipeline";
    const uint32_t id = manager.createPipeline(pipeline_name);
    static_cast<void>(id);

    PipelineManager manager_2(temp_storage_path.string(), 100, 50);
    const std::list<PipelineManager::PipelineDescriptor>& pipelines = manager.getPipelines();

    REQUIRE(pipelines.size() == 1);
    REQUIRE(pipelines.front().name == pipeline_name);
    REQUIRE(pipelines.front().filename == "Some_test_pipeline.json");
  }

  SECTION("NullSinkPipe pipeline is correctly constructed from JSON") {
    const auto json =
        R"({"pipes":{"pipe0":{"type":"NullSinkPipe","parameters":{"max_candidates":10}}}})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "pipe0");
    REQUIRE(std::dynamic_pointer_cast<NullSinkPipe>(pipe->pipe) != nullptr);
  }

  SECTION("NullSinkPipe pipeline is correctly deconstructed to JSON") {
    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    std::shared_ptr<NullSinkPipe> pipe = std::make_shared<NullSinkPipe>(10);
    const std::string name = "null_sink_pipe";
    pipeline->addPipe(name, pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);

    REQUIRE(json.contains("pipes") == true);
    REQUIRE(json["pipes"].size() == 1);
    REQUIRE(json["pipes"].contains(name) == true);
    REQUIRE(json["pipes"][name]["type"].get<std::string>() == "NullSinkPipe");
  }

  SECTION("ResultsSummaryPipe round-trips through JSON serialisation") {
    const auto json = R"({
        "pipes": {
          "summary": {
            "type": "ResultsSummaryPipe",
            "parameters": { "max_candidates": 16, "window_size": 64 }
          }
        }})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();
    REQUIRE(pipes.size() == 1);
    const auto summary_pipe =
        std::dynamic_pointer_cast<ResultsSummaryPipe>(pipes.front()->pipe);
    REQUIRE(summary_pipe != nullptr);
    REQUIRE(summary_pipe->getMaxCandidates() == 16);
    REQUIRE(summary_pipe->getWindowSize() == 64);

    // Round-tripping back to JSON preserves the parameters so the persisted on-disk
    // representation is stable across save/load cycles.
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["summary"]["type"].get<std::string>() == "ResultsSummaryPipe");
    REQUIRE(back["pipes"]["summary"]["parameters"]["max_candidates"].get<uint32_t>() == 16);
    REQUIRE(back["pipes"]["summary"]["parameters"]["window_size"].get<uint32_t>() == 64);
  }

  SECTION("Subroutine sources round-trip through EvaluatorPipe JSON") {
    const auto json = R"({
        "pipes": {
          "callsite": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 4,
              "memory_variables": 16,
              "string_table_items": 0,
              "string_table_item_length": 0,
              "cut_off_score": 0.0,
              "evaluators": [{
                "type": "AdderEvaluator",
                "weight": 1.0,
                "invert_logic": false,
                "parameters": {
                  "trial_count": 1,
                  "value_range": 10,
                  "max_steps_per_trial": 100
                }
              }],
              "subroutines": [
                {
                  "ledger_path": "/tmp/beast-test-sub-a.json",
                  "top_k": 2,
                  "input_arity": 1,
                  "output_arity": 1,
                  "max_steps_per_call": 333
                },
                {
                  "ledger_path": "/tmp/beast-test-sub-b.json",
                  "top_k": 1,
                  "input_arity": 3,
                  "output_arity": 1,
                  "max_steps_per_call": 700
                }
              ]
            }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(eval_pipe != nullptr);
    REQUIRE(eval_pipe->getSubroutineSources().size() == 2);
    REQUIRE(eval_pipe->getSubroutineSources()[0].ledger_path == "/tmp/beast-test-sub-a.json");
    REQUIRE(eval_pipe->getSubroutineSources()[0].top_k == 2);
    REQUIRE(eval_pipe->getSubroutineSources()[1].input_arity == 3);
    REQUIRE(eval_pipe->getSubroutineSources()[1].max_steps_per_call == 700);

    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["callsite"]["parameters"]["subroutines"].size() == 2);
    REQUIRE(back["pipes"]["callsite"]["parameters"]["subroutines"][0]["ledger_path"]
                .get<std::string>() == "/tmp/beast-test-sub-a.json");
    REQUIRE(back["pipes"]["callsite"]["parameters"]["subroutines"][1]["output_arity"]
                .get<uint32_t>() == 1);
  }

  SECTION("AdderEvaluator round-trips through EvaluatorPipe JSON") {
    const auto json = R"({
        "pipes": {
          "adder": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 10,
              "memory_variables": 16,
              "string_table_items": 0,
              "string_table_item_length": 0,
              "cut_off_score": 0.0,
              "evaluators": [{
                "type": "AdderEvaluator",
                "weight": 1.0,
                "invert_logic": false,
                "parameters": {
                  "trial_count": 6,
                  "value_range": 42,
                  "max_steps_per_trial": 555
                }
              }]
            }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(eval_pipe != nullptr);
    const auto descs = eval_pipe->getEvaluators();
    REQUIRE(descs.size() == 1);
    const auto adder = std::dynamic_pointer_cast<AdderEvaluator>(descs.front().evaluator);
    REQUIRE(adder != nullptr);
    REQUIRE(adder->getTrialCount() == 6);
    REQUIRE(adder->getValueRange() == 42);
    REQUIRE(adder->getMaxStepsPerTrial() == 555);
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["adder"]["parameters"]["evaluators"][0]["type"].get<std::string>() ==
            "AdderEvaluator");
    REQUIRE(back["pipes"]["adder"]["parameters"]["evaluators"][0]["parameters"]["trial_count"]
                .get<uint32_t>() == 6);
  }

  SECTION("MaximumEvaluator round-trips through EvaluatorPipe JSON") {
    const auto json = R"({
        "pipes": {
          "maxer": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8,
              "memory_variables": 32,
              "string_table_items": 0,
              "string_table_item_length": 0,
              "cut_off_score": 0.0,
              "evaluators": [{
                "type": "MaximumEvaluator",
                "weight": 1.0,
                "invert_logic": false,
                "parameters": {
                  "input_count": 4,
                  "trial_count": 5,
                  "value_range": 99,
                  "max_steps_per_trial": 777
                }
              }]
            }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(eval_pipe != nullptr);
    const auto descs = eval_pipe->getEvaluators();
    REQUIRE(descs.size() == 1);
    const auto maximum = std::dynamic_pointer_cast<MaximumEvaluator>(descs.front().evaluator);
    REQUIRE(maximum != nullptr);
    REQUIRE(maximum->getInputCount() == 4);
    REQUIRE(maximum->getTrialCount() == 5);
    REQUIRE(maximum->getValueRange() == 99);
    REQUIRE(maximum->getMaxStepsPerTrial() == 777);
  }

  SECTION("ScoreGraphPipe round-trips through PipelineManager JSON") {
    // Same shape as the ResultsSummaryPipe coverage above: parse a single-pipe pipeline,
    // confirm the parameters survive construction, then re-serialise and confirm the
    // on-wire form is canonical. The optional parameters (`window_seconds`,
    // `max_samples`) must round-trip exactly (no silent defaulting) when explicitly set.
    const auto json = R"({
        "pipes": {
          "graph": {
            "type": "ScoreGraphPipe",
            "parameters": {
              "max_candidates": 32,
              "window_seconds": 12.5,
              "max_samples": 200
            }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto graph_pipe = std::dynamic_pointer_cast<ScoreGraphPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(graph_pipe != nullptr);
    REQUIRE(graph_pipe->getMaxCandidates() == 32);
    REQUIRE(graph_pipe->getWindowSeconds() == Approx(12.5));
    REQUIRE(graph_pipe->getMaxSamples() == 200);

    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["graph"]["type"].get<std::string>() == "ScoreGraphPipe");
    REQUIRE(back["pipes"]["graph"]["parameters"]["max_candidates"].get<uint32_t>() == 32);
    REQUIRE(back["pipes"]["graph"]["parameters"]["window_seconds"].get<double>() ==
            Approx(12.5));
    REQUIRE(back["pipes"]["graph"]["parameters"]["max_samples"].get<uint32_t>() == 200);
  }

  SECTION("ScoreGraphPipe optional parameters default cleanly when omitted") {
    // Legacy/minimal JSON: only `max_candidates` is present. The parser must fall back
    // to the implementation defaults (60s window, 1024 samples) so a hand-written
    // minimal pipeline still loads.
    const auto json = R"({
        "pipes": {
          "graph": {
            "type": "ScoreGraphPipe",
            "parameters": { "max_candidates": 50 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto graph_pipe = std::dynamic_pointer_cast<ScoreGraphPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(graph_pipe != nullptr);
    REQUIRE(graph_pipe->getWindowSeconds() == Approx(60.0));
    REQUIRE(graph_pipe->getMaxSamples() == 1024);
  }

  SECTION("Sha256RoundEvaluator round-trips through EvaluatorPipe JSON") {
    // End-to-end JSON round-trip check: parse a pipeline whose only evaluator is the new
    // SHA-256 round evaluator, confirm the parameters survive the trip, and then
    // re-serialise and confirm the on-wire shape matches.
    const auto json = R"({
        "pipes": {
          "sha256_round": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 16,
              "memory_variables": 32,
              "string_table_items": 0,
              "string_table_item_length": 0,
              "cut_off_score": 0.0,
              "evaluators": [{
                "type": "Sha256RoundEvaluator",
                "weight": 1.0,
                "invert_logic": false,
                "parameters": {
                  "trial_count": 5,
                  "round_constant_index": 7,
                  "max_steps_per_trial": 3500
                }
              }]
            }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(eval_pipe != nullptr);
    const auto descs = eval_pipe->getEvaluators();
    REQUIRE(descs.size() == 1);
    const auto sha = std::dynamic_pointer_cast<Sha256RoundEvaluator>(descs.front().evaluator);
    REQUIRE(sha != nullptr);
    REQUIRE(sha->getTrialCount() == 5);
    REQUIRE(sha->getRoundConstantIndex() == 7);
    REQUIRE(sha->getMaxStepsPerTrial() == 3500);
    // K[7] is one of the well-known FIPS 180-4 round constants; we exercise the
    // constants-table lookup path here so a future refactor that swaps tables or
    // accidentally truncates the array can't slip past unnoticed.
    REQUIRE(sha->getRoundConstantValue() == 0xab1c5ed5U);
    // The mode key is intentionally omitted from the input JSON above; the parser must
    // fall back to Fixed so legacy ledgers (predating the mode parameter) keep their
    // original scoring behaviour after upgrading.
    REQUIRE(sha->getRoundConstantsMode() ==
            Sha256RoundEvaluator::RoundConstantsMode::Fixed);

    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["type"]
                .get<std::string>() == "Sha256RoundEvaluator");
    REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["parameters"]
                ["round_constant_index"].get<uint32_t>() == 7);
    REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["parameters"]
                ["max_steps_per_trial"].get<uint32_t>() == 3500);
    // Even legacy ledgers gain the explicit mode key on the way out, so the next load
    // sees the canonical form regardless of which version produced it.
    REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["parameters"]
                ["round_constants_mode"].get<std::string>() == "fixed");
    // Same story for `rounds_per_trial`: legacy ledgers omit it, parser defaults to 1,
    // serializer emits it explicitly so the on-wire form is canonical going forward.
    REQUIRE(sha->getRoundsPerTrial() == 1);
    REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["parameters"]
                ["rounds_per_trial"].get<uint32_t>() == 1);
  }

  SECTION("Sha256RoundEvaluator round-trips rounds_per_trial when explicitly set") {
    // Explicit non-default `rounds_per_trial` must survive the round-trip intact;
    // exercising values at both ends of the legal range (2 = minimum multi-round, 64 =
    // max-clamp boundary) catches off-by-one mistakes in either direction.
    for (const uint32_t requested : {static_cast<uint32_t>(2), static_cast<uint32_t>(64)}) {
      const std::string template_json = R"({
          "pipes": {
            "sha256_round": {
              "type": "EvaluatorPipe",
              "parameters": {
                "max_candidates": 16,
                "memory_variables": 32,
                "string_table_items": 0,
                "string_table_item_length": 0,
                "cut_off_score": 0.0,
                "evaluators": [{
                  "type": "Sha256RoundEvaluator",
                  "weight": 1.0,
                  "invert_logic": false,
                  "parameters": {
                    "trial_count": 3,
                    "round_constant_index": 0,
                    "rounds_per_trial": __ROUNDS__,
                    "max_steps_per_trial": 200
                  }
                }]
              }
            }
          }})";
      auto json_body = template_json;
      json_body.replace(json_body.find("__ROUNDS__"), std::string("__ROUNDS__").size(),
                        std::to_string(requested));
      const auto json = nlohmann::json::parse(json_body);
      const auto pipeline = PipelineManager::constructPipelineFromJson(json);
      const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(
          pipeline->getPipes().front()->pipe);
      REQUIRE(eval_pipe != nullptr);
      const auto sha = std::dynamic_pointer_cast<Sha256RoundEvaluator>(
          eval_pipe->getEvaluators().front().evaluator);
      REQUIRE(sha != nullptr);
      REQUIRE(sha->getRoundsPerTrial() == requested);
      const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
      REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["parameters"]
                  ["rounds_per_trial"].get<uint32_t>() == requested);
    }
  }

  SECTION("Sha256RoundEvaluator round-trips the cycle_all and random_per_trial modes") {
    // Mirror image of the section above but with each non-default mode explicitly set in
    // the JSON. The parser has to recognise the strings, the evaluator has to retain the
    // mode through construction, and the serialiser has to emit the same string back.
    // Templated raw-string + simple substitution keeps the JSON readable; using
    // nlohmann::json's initializer-list constructor for a structure this nested was a
    // brace-counting trap.
    const std::string json_template = R"({
        "pipes": {
          "sha256_round": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 16,
              "memory_variables": 32,
              "string_table_items": 0,
              "string_table_item_length": 0,
              "cut_off_score": 0.0,
              "evaluators": [{
                "type": "Sha256RoundEvaluator",
                "weight": 1.0,
                "invert_logic": false,
                "parameters": {
                  "trial_count": 3,
                  "round_constant_index": 1,
                  "round_constants_mode": "__MODE__",
                  "max_steps_per_trial": 200
                }
              }]
            }
          }
        }})";

    const std::vector<std::pair<std::string, Sha256RoundEvaluator::RoundConstantsMode>>
        cases{{"cycle_all", Sha256RoundEvaluator::RoundConstantsMode::CycleAll},
              {"random_per_trial",
               Sha256RoundEvaluator::RoundConstantsMode::RandomPerTrial}};
    for (const auto& [mode_string, expected_enum] : cases) {
      auto json_body = json_template;
      json_body.replace(json_body.find("__MODE__"), std::string("__MODE__").size(),
                        mode_string);
      const auto json = nlohmann::json::parse(json_body);

      const auto pipeline = PipelineManager::constructPipelineFromJson(json);
      const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(
          pipeline->getPipes().front()->pipe);
      REQUIRE(eval_pipe != nullptr);
      const auto sha = std::dynamic_pointer_cast<Sha256RoundEvaluator>(
          eval_pipe->getEvaluators().front().evaluator);
      REQUIRE(sha != nullptr);
      REQUIRE(sha->getRoundConstantsMode() == expected_enum);
      const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
      REQUIRE(back["pipes"]["sha256_round"]["parameters"]["evaluators"][0]["parameters"]
                  ["round_constants_mode"].get<std::string>() == mode_string);
    }
  }

  SECTION("Sha256RoundEvaluator rejects unknown round_constants_mode strings") {
    // A typo'd mode string is much more likely than a deliberate wrong value, so we want
    // the parser to surface it loudly rather than silently picking a default. The error
    // message has to name the offending value (so the user can locate it) and list the
    // accepted values (so they don't have to dig through docs).
    const auto json = R"({
        "pipes": {
          "sha256_round": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 16,
              "memory_variables": 32,
              "string_table_items": 0,
              "string_table_item_length": 0,
              "cut_off_score": 0.0,
              "evaluators": [{
                "type": "Sha256RoundEvaluator",
                "weight": 1.0,
                "invert_logic": false,
                "parameters": {
                  "trial_count": 3,
                  "round_constant_index": 0,
                  "round_constants_mode": "all_of_them_at_once",
                  "max_steps_per_trial": 200
                }
              }]
            }
          }
        }})"_json;
    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json),
                      std::invalid_argument);
  }

  SECTION("Curriculum evaluators round-trip through EvaluatorPipe JSON") {
    // One section to cover all six curriculum evaluators -- they share the same JSON
    // shape (type + parameters), so a single sweep is enough to catch
    // typo/serialisation bugs in any of them. Each subsection picks parameters that
    // exercise the type-specific knobs (enum decoding, clamp behaviour, etc.).
    const auto json = R"({
        "pipes": {
          "identity": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8, "memory_variables": 32, "string_table_items": 0,
              "string_table_item_length": 0, "cut_off_score": 0.0,
              "evaluators": [{
                "type": "IdentityEvaluator", "weight": 1.0, "invert_logic": false,
                "parameters": { "trial_count": 6, "width": 5, "max_steps_per_trial": 1200 }
              }]
            }
          },
          "bitwise": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8, "memory_variables": 16, "string_table_items": 0,
              "string_table_item_length": 0, "cut_off_score": 0.0,
              "evaluators": [{
                "type": "BitwiseEvaluator", "weight": 1.0, "invert_logic": false,
                "parameters": { "trial_count": 4, "operation": "and",
                                "max_steps_per_trial": 900 }
              }]
            }
          },
          "rotate": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8, "memory_variables": 16, "string_table_items": 0,
              "string_table_item_length": 0, "cut_off_score": 0.0,
              "evaluators": [{
                "type": "RotateEvaluator", "weight": 1.0, "invert_logic": false,
                "parameters": { "trial_count": 4, "amount": 13, "direction": "right",
                                "max_steps_per_trial": 900 }
              }]
            }
          },
          "sigma": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8, "memory_variables": 16, "string_table_items": 0,
              "string_table_item_length": 0, "cut_off_score": 0.0,
              "evaluators": [{
                "type": "Sha256SigmaEvaluator", "weight": 1.0, "invert_logic": false,
                "parameters": { "trial_count": 4, "variant": "small1",
                                "max_steps_per_trial": 1500 }
              }]
            }
          },
          "ch": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8, "memory_variables": 16, "string_table_items": 0,
              "string_table_item_length": 0, "cut_off_score": 0.0,
              "evaluators": [{
                "type": "Sha256ChEvaluator", "weight": 1.0, "invert_logic": false,
                "parameters": { "trial_count": 4, "max_steps_per_trial": 1100 }
              }]
            }
          },
          "maj": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 8, "memory_variables": 16, "string_table_items": 0,
              "string_table_item_length": 0, "cut_off_score": 0.0,
              "evaluators": [{
                "type": "Sha256MajEvaluator", "weight": 1.0, "invert_logic": false,
                "parameters": { "trial_count": 4, "max_steps_per_trial": 1100 }
              }]
            }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);

    auto evaluator_in = [&](const std::string& pipe_name) -> std::shared_ptr<Evaluator> {
      for (const auto& managed : pipeline->getPipes()) {
        if (managed->name == pipe_name) {
          const auto ep = std::dynamic_pointer_cast<EvaluatorPipe>(managed->pipe);
          REQUIRE(ep != nullptr);
          REQUIRE(ep->getEvaluators().size() == 1);
          return ep->getEvaluators().front().evaluator;
        }
      }
      return nullptr;
    };

    const auto id_eval = std::dynamic_pointer_cast<IdentityEvaluator>(evaluator_in("identity"));
    REQUIRE(id_eval != nullptr);
    CHECK(id_eval->getWidth() == 5);
    CHECK(id_eval->getTrialCount() == 6);

    const auto bw_eval = std::dynamic_pointer_cast<BitwiseEvaluator>(evaluator_in("bitwise"));
    REQUIRE(bw_eval != nullptr);
    CHECK(bw_eval->getOperation() == BitwiseEvaluator::Operation::And);

    const auto rot_eval = std::dynamic_pointer_cast<RotateEvaluator>(evaluator_in("rotate"));
    REQUIRE(rot_eval != nullptr);
    CHECK(rot_eval->getAmount() == 13);
    CHECK(rot_eval->getDirection() == RotateEvaluator::Direction::Right);

    const auto sigma_eval =
        std::dynamic_pointer_cast<Sha256SigmaEvaluator>(evaluator_in("sigma"));
    REQUIRE(sigma_eval != nullptr);
    CHECK(sigma_eval->getVariant() == Sha256SigmaEvaluator::Variant::SmallSigma1);

    REQUIRE(std::dynamic_pointer_cast<Sha256ChEvaluator>(evaluator_in("ch")) != nullptr);
    REQUIRE(std::dynamic_pointer_cast<Sha256MajEvaluator>(evaluator_in("maj")) != nullptr);

    // Re-serialise and confirm the type/parameter names survive the trip back. We
    // spot-check the trickier shapes (the ones with enum-as-string values), since the
    // numeric-only ones are covered by the parameter-value checks above.
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    CHECK(back["pipes"]["bitwise"]["parameters"]["evaluators"][0]["parameters"]["operation"]
              .get<std::string>() == "and");
    CHECK(back["pipes"]["rotate"]["parameters"]["evaluators"][0]["parameters"]["direction"]
              .get<std::string>() == "right");
    CHECK(back["pipes"]["sigma"]["parameters"]["evaluators"][0]["parameters"]["variant"]
              .get<std::string>() == "small1");
  }

  SECTION("FanPipe round-trips through JSON serialisation") {
    const auto json = R"({
        "pipes": {
          "fan": {
            "type": "FanPipe",
            "parameters": { "max_candidates": 32, "window_seconds": 5.0 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto fan = std::dynamic_pointer_cast<FanPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(fan != nullptr);
    REQUIRE(fan->getWindowSeconds() == Approx(5.0));
    REQUIRE(fan->getMaxCandidates() == 32);
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["fan"]["type"].get<std::string>() == "FanPipe");
    REQUIRE(back["pipes"]["fan"]["parameters"]["window_seconds"].get<double>() == Approx(5.0));
  }

  SECTION("FanPipe defaults its window when window_seconds omitted") {
    const auto json = R"({
        "pipes": {
          "fan": {
            "type": "FanPipe",
            "parameters": { "max_candidates": 8 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto fan = std::dynamic_pointer_cast<FanPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(fan != nullptr);
    REQUIRE(fan->getWindowSeconds() == Approx(2.0));
  }

  SECTION("MultiplexerPipe round-trips through JSON serialisation") {
    const auto json = R"({
        "pipes": {
          "mux": {
            "type": "MultiplexerPipe",
            "parameters": { "max_candidates": 20, "input_slots": 4 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto mux = std::dynamic_pointer_cast<MultiplexerPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(mux != nullptr);
    REQUIRE(mux->getInputSlots() == 4);
    REQUIRE(mux->getInputSlotCount() == 4);
    REQUIRE(mux->getOutputSlotCount() == 1);
    REQUIRE(mux->getMaxCandidates() == 20);
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["mux"]["type"].get<std::string>() == "MultiplexerPipe");
    REQUIRE(back["pipes"]["mux"]["parameters"]["max_candidates"].get<uint32_t>() == 20);
    REQUIRE(back["pipes"]["mux"]["parameters"]["input_slots"].get<uint32_t>() == 4);
  }

  SECTION("DemultiplexerPipe round-trips both strategies through JSON") {
    for (const std::string strategy : {"round_robin", "broadcast"}) {
      INFO("strategy = " << strategy);
      const auto json = nlohmann::json::parse(
          R"({"pipes":{"dmx":{"type":"DemultiplexerPipe","parameters":{"max_candidates":12,"output_slots":3,"strategy":")" +
          strategy + R"("}}}})");
      const auto pipeline = PipelineManager::constructPipelineFromJson(json);
      const auto dmx = std::dynamic_pointer_cast<DemultiplexerPipe>(
          pipeline->getPipes().front()->pipe);
      REQUIRE(dmx != nullptr);
      REQUIRE(dmx->getOutputSlots() == 3);
      REQUIRE(dmx->getOutputSlotCount() == 3);
      REQUIRE(dmx->getInputSlotCount() == 1);
      REQUIRE(dmx->getMaxCandidates() == 12);
      const bool expected_broadcast = strategy == "broadcast";
      REQUIRE((dmx->getStrategy() == DemultiplexerPipe::Strategy::Broadcast) ==
              expected_broadcast);
      const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
      REQUIRE(back["pipes"]["dmx"]["parameters"]["strategy"].get<std::string>() == strategy);
    }
  }

  SECTION("FilterPipe round-trips threshold through JSON") {
    const auto json = R"({
        "pipes": {
          "filt": {
            "type": "FilterPipe",
            "parameters": { "max_candidates": 24, "threshold": 0.42 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto filt = std::dynamic_pointer_cast<FilterPipe>(pipeline->getPipes().front()->pipe);
    REQUIRE(filt != nullptr);
    REQUIRE(filt->getMaxCandidates() == 24);
    REQUIRE(filt->getThreshold() == Approx(0.42));
    REQUIRE(filt->getInputSlotCount() == 1);
    REQUIRE(filt->getOutputSlotCount() == 2);
    const auto back = PipelineManager::deconstructPipelineToJson(pipeline);
    REQUIRE(back["pipes"]["filt"]["type"].get<std::string>() == "FilterPipe");
    REQUIRE(back["pipes"]["filt"]["parameters"]["max_candidates"].get<uint32_t>() == 24);
    REQUIRE(back["pipes"]["filt"]["parameters"]["threshold"].get<double>() == Approx(0.42));
  }

  SECTION("DemultiplexerPipe defaults to round_robin when strategy omitted") {
    const auto json = R"({
        "pipes": {
          "dmx": {
            "type": "DemultiplexerPipe",
            "parameters": { "max_candidates": 4, "output_slots": 2 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto dmx = std::dynamic_pointer_cast<DemultiplexerPipe>(
        pipeline->getPipes().front()->pipe);
    REQUIRE(dmx != nullptr);
    REQUIRE(dmx->getStrategy() == DemultiplexerPipe::Strategy::RoundRobin);
  }

  SECTION("ResultsSummaryPipe defaults window_size when omitted") {
    // Omitting `window_size` should pick the built-in default (256) rather than throwing
    // -- the UI exposes the knob as optional and persists 0 when the user leaves it
    // blank.
    const auto json = R"({
        "pipes": {
          "summary": {
            "type": "ResultsSummaryPipe",
            "parameters": { "max_candidates": 4 }
          }
        }})"_json;
    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto summary_pipe =
        std::dynamic_pointer_cast<ResultsSummaryPipe>(pipeline->getPipes().front()->pipe);
    REQUIRE(summary_pipe != nullptr);
    REQUIRE(summary_pipe->getWindowSize() == 256);
  }

  SECTION("EvaluatorPipe+MazeEvaluator pipeline is correctly constructed from JSON") {
    const auto json = R"({
        "pipes": {
          "eval_pipe": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 10,
              "memory_variables": 5,
              "string_table_items": 2,
              "string_table_item_length": 25,
              "evaluators": [
                {
                  "type": "MazeEvaluator",
                  "parameters": {
                    "rows": 10,
                    "cols": 12,
                    "difficulty": 0.61,
                    "max_steps": 1325
                  },
                  "weight": 1.0,
                  "invert_logic": false
                }
              ]
            }
          }
        }})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "eval_pipe");
    const auto eval_pipe = std::dynamic_pointer_cast<EvaluatorPipe>(pipe->pipe);
    REQUIRE(eval_pipe != nullptr);
    const auto& evaluators = eval_pipe->getEvaluators();
    REQUIRE(evaluators.size() == 1);
    const auto& maze_eval_desc = evaluators.front();
    REQUIRE(maze_eval_desc.weight == 1.0);
    REQUIRE(maze_eval_desc.invert_logic == false);
    const auto maze_eval = std::dynamic_pointer_cast<MazeEvaluator>(maze_eval_desc.evaluator);
    REQUIRE(maze_eval != nullptr);
    REQUIRE(maze_eval->getRows() == 10);
    REQUIRE(maze_eval->getCols() == 12);
    REQUIRE(maze_eval->getDifficulty() == 0.61);
    REQUIRE(maze_eval->getMaxSteps() == 1325);
  }

  SECTION("EvaluatorPipe+MazeEvaluator pipeline is correctly deconstructed to JSON") {
    // Evaluator pipe parameters
    const uint32_t max_candidates = 128;
    const uint32_t memory_size = 12;
    const uint32_t string_table_items = 3;
    const uint32_t string_table_item_length = 25;
    const std::string name = "maze_eval_pipe";

    // Maze evaluator parameters
    const uint32_t rows = 99;
    const uint32_t cols = 51;
    const double difficulty = 0.73;
    const uint32_t max_steps = 1024;

    const double weight = 0.57;
    const bool invert_logic = false;

    std::shared_ptr<EvaluatorPipe> pipe = std::make_shared<EvaluatorPipe>(
        max_candidates, memory_size, string_table_items, string_table_item_length);
    std::shared_ptr<Evaluator> eval =
        std::make_shared<MazeEvaluator>(rows, cols, difficulty, max_steps);
    pipe->addEvaluator(eval, weight, invert_logic);

    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    pipeline->addPipe(name, pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);

    REQUIRE(json.contains("pipes") == true);
    REQUIRE(json["pipes"].size() == 1);
    REQUIRE(json["pipes"].contains(name) == true);
    REQUIRE(json["pipes"][name]["type"].get<std::string>() == "EvaluatorPipe");
    REQUIRE(json["pipes"][name]["parameters"]["max_candidates"] == max_candidates);
    REQUIRE(json["pipes"][name]["parameters"]["memory_variables"] == memory_size);
    REQUIRE(json["pipes"][name]["parameters"]["string_table_items"] == string_table_items);
    REQUIRE(json["pipes"][name]["parameters"]["string_table_item_length"] ==
            string_table_item_length);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"].size() == 1);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["type"] == "MazeEvaluator");
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["weight"] == weight);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["invert_logic"] == invert_logic);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["max_steps"] ==
            max_steps);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["difficulty"] ==
            difficulty);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["rows"] == rows);
    REQUIRE(json["pipes"][name]["parameters"]["evaluators"][0]["parameters"]["cols"] == cols);
  }

  SECTION("EvaluatorPipe+MazeEvaluator pipeline reconstruction through JSON works") {
    // Evaluator pipe parameters
    const uint32_t max_candidates = 128;
    const uint32_t memory_size = 12;
    const uint32_t string_table_items = 3;
    const uint32_t string_table_item_length = 25;
    const std::string name = "maze_eval_pipe";

    // Maze evaluator parameters
    const uint32_t rows = 99;
    const uint32_t cols = 51;
    const double difficulty = 0.73;
    const uint32_t max_steps = 1024;

    const double weight = 0.57;
    const bool invert_logic = false;

    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    {
      std::shared_ptr<EvaluatorPipe> pipe = std::make_shared<EvaluatorPipe>(
          max_candidates, memory_size, string_table_items, string_table_item_length);
      std::shared_ptr<Evaluator> eval =
          std::make_shared<MazeEvaluator>(rows, cols, difficulty, max_steps);
      pipe->addEvaluator(eval, weight, invert_logic);
      pipeline->addPipe(name, pipe);
    }

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);
    const auto reconstructed_pipeline = PipelineManager::constructPipelineFromJson(json);

    {
      const auto& managed_pipes = reconstructed_pipeline->getPipes();
      REQUIRE(managed_pipes.size() == 1);
      const auto& managed_pipe = managed_pipes.front();
      REQUIRE(managed_pipe->name == name);
      const auto pipe = std::dynamic_pointer_cast<EvaluatorPipe>(managed_pipe->pipe);
      REQUIRE(pipe->getMemorySize() == memory_size);
      REQUIRE(pipe->getMaxCandidates() == max_candidates);
      REQUIRE(pipe->getStringTableSize() == string_table_items);
      REQUIRE(pipe->getStringTableItemLength() == string_table_item_length);
      REQUIRE(pipe->getStringTableSize() == string_table_items);

      const auto& evaluator_descriptions = pipe->getEvaluators();
      REQUIRE(evaluator_descriptions.size() == 1);
      const auto& evaluator_description = evaluator_descriptions.front();
      REQUIRE(evaluator_description.weight == weight);
      REQUIRE(evaluator_description.invert_logic == invert_logic);
      const auto evaluator =
          std::dynamic_pointer_cast<MazeEvaluator>(evaluator_description.evaluator);
      REQUIRE(evaluator != nullptr);
      REQUIRE(evaluator->getRows() == rows);
      REQUIRE(evaluator->getCols() == cols);
      REQUIRE(evaluator->getDifficulty() == difficulty);
      REQUIRE(evaluator->getMaxSteps() == max_steps);
    }
  }

  SECTION("EvaluatorPipe round-trips EvolutionParameters and cut_off_score through JSON") {
    // Regression test for the previously-empty EvolutionPipe JSON branch: ensure both the
    // GA knobs and the cut-off score survive a deconstruct/reconstruct cycle, including the
    // OpcodeWeights map.
    std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
    auto pipe = std::make_shared<EvaluatorPipe>(/*max_candidates=*/16,
                                                /*memory_variables=*/8,
                                                /*string_table_items=*/2,
                                                /*string_table_item_length=*/4);
    pipe->addEvaluator(std::make_shared<MazeEvaluator>(5, 5, 0.2, 100), 1.0, false);
    pipe->setCutOffScore(0.42);

    EvolutionPipe::EvolutionParameters params;
    params.generations = 7;
    params.crossover_probability = 0.6;
    params.mutation_probability = 0.15;
    params.elitism = false;
    params.byte_mutation_share = 0.33;
    params.variable_count = 32;
    params.string_table_size = 3;
    params.string_table_item_length = 7;
    params.max_genome_bytes = 512;
    params.starting_program_size = 48;
    params.opcode_weights[OpCode::NoOp] = 0.0;
    params.opcode_weights[OpCode::SetVariable] = 4.5;
    params.opcode_weights[OpCode::Terminate] = 0.1;
    pipe->setEvolutionParameters(params);

    pipeline->addPipe("eval", pipe);

    const auto json = PipelineManager::deconstructPipelineToJson(pipeline);
    const auto rebuilt = PipelineManager::constructPipelineFromJson(json);
    const auto rebuilt_pipe =
        std::dynamic_pointer_cast<EvaluatorPipe>(rebuilt->getPipes().front()->pipe);
    REQUIRE(rebuilt_pipe != nullptr);
    REQUIRE(rebuilt_pipe->getCutOffScore() == 0.42);

    const auto& rebuilt_params = rebuilt_pipe->getEvolutionParameters();
    REQUIRE(rebuilt_params.generations == params.generations);
    REQUIRE(rebuilt_params.crossover_probability == params.crossover_probability);
    REQUIRE(rebuilt_params.mutation_probability == params.mutation_probability);
    REQUIRE(rebuilt_params.elitism == params.elitism);
    REQUIRE(rebuilt_params.byte_mutation_share == params.byte_mutation_share);
    REQUIRE(rebuilt_params.variable_count == params.variable_count);
    REQUIRE(rebuilt_params.string_table_size == params.string_table_size);
    REQUIRE(rebuilt_params.string_table_item_length == params.string_table_item_length);
    REQUIRE(rebuilt_params.max_genome_bytes == params.max_genome_bytes);
    REQUIRE(rebuilt_params.starting_program_size == params.starting_program_size);
    REQUIRE(rebuilt_params.opcode_weights.size() == params.opcode_weights.size());
    for (const auto& [opcode, weight] : params.opcode_weights) {
      REQUIRE(rebuilt_params.opcode_weights.count(opcode) == 1);
      REQUIRE(rebuilt_params.opcode_weights.at(opcode) == weight);
    }
  }

  SECTION("EvaluatorPipe with invalid evaluator pipeline constructed from JSON throw") {
    const auto json = R"({
        "pipes": {
          "eval_pipe": {
            "type": "EvaluatorPipe",
            "parameters": {
              "max_candidates": 10,
              "memory_variables": 5,
              "string_table_items": 2,
              "string_table_item_length": 25,
              "evaluators": [
                {
                  "type": "Some invalid evaluator",
                  "weight": 1.0,
                  "invert_logic": false
                }
              ]
            }
          }
        }})"_json;

    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json), std::invalid_argument);
  }

  SECTION("Pipe without type in pipeline constructed from JSON throw") {
    const auto json = R"({
        "pipes": {
          "eval_pipe": {
          }
        }})"_json;

    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json), std::invalid_argument);
  }

  SECTION("ProgramFactoryPipe pipeline is correctly constructed from JSON") {
    const auto json = R"({
        "pipes": {
          "factory_pipe": {
            "type": "ProgramFactoryPipe",
            "parameters": {
              "max_candidates": 11,
              "max_size": 100,
              "memory_variables": 3,
              "string_table_items": 20,
              "string_table_item_length": 71,
              "factory": "RandomProgramFactory"
            }
          }
        }})"_json;

    const auto pipeline = PipelineManager::constructPipelineFromJson(json);
    const auto& pipes = pipeline->getPipes();

    REQUIRE(pipes.size() == 1);
    const auto& pipe = pipes.front();
    REQUIRE(pipe->name == "factory_pipe");
    const auto factory_pipe = std::dynamic_pointer_cast<ProgramFactoryPipe>(pipe->pipe);
    REQUIRE(factory_pipe != nullptr);
    REQUIRE(factory_pipe->getMaxCandidates() == 11);
    REQUIRE(factory_pipe->getMaxSize() == 100);
    REQUIRE(factory_pipe->getMemorySize() == 3);
    REQUIRE(factory_pipe->getStringTableSize() == 20);
    REQUIRE(factory_pipe->getStringTableItemLength() == 71);
    const auto& factory =
        std::dynamic_pointer_cast<RandomProgramFactory>(factory_pipe->getFactory());
    REQUIRE(factory != nullptr);
  }

  SECTION("ProgramFactoryPipe pipeline with invalid factory type from JSON throws") {
    const auto json = R"({
        "pipes": {
          "factory_pipe": {
            "type": "ProgramFactoryPipe",
            "parameters": {
              "max_candidates": 11,
              "max_size": 100,
              "memory_variables": 3,
              "string_table_items": 20,
              "string_table_item_length": 71,
              "factory": "Some invalid factory type"
            }
          }
        }})"_json;

    REQUIRE_THROWS_AS(PipelineManager::constructPipelineFromJson(json), std::invalid_argument);
  }

  SECTION("mutatePipeline adds, connects, and removes pipes via JSON edits") {
    // Drives the same JSON shape the HTTP layer's add_pipe / add_connection / delete_pipe
    // / delete_connection handlers produce, end-to-end through the manager. Validates that
    // each edit reaches the live Pipeline object (not just the on-disk model).
    const uint32_t id = manager.createPipeline("ui-test");

    // 1. Add a ProgramFactoryPipe.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& metadata) {
      model["pipes"]["factory"] = R"({
        "type": "ProgramFactoryPipe",
        "parameters": {
          "factory": "RandomProgramFactory",
          "max_candidates": 5,
          "max_size": 32,
          "memory_variables": 16,
          "string_table_items": 0,
          "string_table_item_length": 0
        }
      })"_json;
      metadata["pipes"]["factory"]["position"] = {{"x", 10}, {"y", 20}};
    });
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == 1);
    REQUIRE(manager.getPipelineById(id).metadata["pipes"]["factory"]["position"]["x"] == 10);

    // 2. Add a NullSinkPipe and connect them.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["sink"] = R"({
        "type": "NullSinkPipe",
        "parameters": { "max_candidates": 10 }
      })"_json;
      if (!model.contains("connections") || !model["connections"].is_array()) {
        model["connections"] = nlohmann::json::array();
      }
      model["connections"].push_back(R"({
        "source_pipe": "factory",
        "source_slot": 0,
        "destination_pipe": "sink",
        "destination_slot": 0,
        "buffer_size": 4
      })"_json);
    });
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == 2);
    REQUIRE(manager.getPipelineById(id).pipeline->getConnections().size() == 1);

    // 3. Delete the factory; the connection referencing it must come along to satisfy
    //    constructPipelineFromJson, otherwise rebuild would throw.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& metadata) {
      model["pipes"].erase("factory");
      auto& connections = model["connections"];
      connections.erase(std::remove_if(connections.begin(), connections.end(),
                                       [](const nlohmann::json& c) {
                                         return c["source_pipe"] == "factory" ||
                                                c["destination_pipe"] == "factory";
                                       }),
                        connections.end());
      metadata["pipes"].erase("factory");
    });
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == 1);
    REQUIRE(manager.getPipelineById(id).pipeline->getConnections().empty());
  }

  SECTION("Example JSON pipelines parse cleanly") {
    // Smoke-test the on-disk example pipelines so a future schema change can't quietly
    // break them. We resolve the path relative to the source tree -- the build dir's
    // copy isn't always present and we want to fail loudly if the examples drift away
    // from the constructor's expectations.
    const std::filesystem::path src_root =
        std::filesystem::path(__FILE__).parent_path().parent_path();
    for (const auto* sample :
         {"ascending-mazes.json", "survivor-recirculation.json", "sha256-round.json",
          "sha256-round-multi-k.json", "sha256-curriculum.json",
          "sha256-round-with-subroutines.json", "primitives-gym.json",
          "maze-ladder-with-subroutines.json", "cognitive-scaffolding-ab.json",
          "adder-with-filter.json"}) {
      INFO(sample);
      const auto path = src_root / "examples" / "compose-pipelines" / sample;
      REQUIRE(std::filesystem::exists(path));
      std::ifstream input(path);
      REQUIRE(input.is_open());
      nlohmann::json wrapped;
      input >> wrapped;
      REQUIRE(wrapped.contains("model"));
      const auto pipeline = PipelineManager::constructPipelineFromJson(wrapped["model"]);
      REQUIRE(pipeline != nullptr);
      // Each example wires at least one connection -- a "pipeline" with no edges is a
      // sign the file got truncated.
      REQUIRE(!pipeline->getConnections().empty());
    }
  }

  SECTION("mutatePipeline refuses to mutate a running pipeline") {
    const uint32_t id = manager.createPipeline("running-test");
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["sink"] = R"({
        "type": "NullSinkPipe",
        "parameters": { "max_candidates": 4 }
      })"_json;
    });
    manager.getPipelineById(id).pipeline->start();
    REQUIRE_THROWS_AS(manager.mutatePipeline(id,
                                             [](nlohmann::json& model,
                                                nlohmann::json& /*metadata*/) {
                                               model["pipes"]["sink2"] = R"({
            "type": "NullSinkPipe", "parameters": { "max_candidates": 1 }
          })"_json;
                                             }),
                      std::invalid_argument);
    manager.getPipelineById(id).pipeline->stop();
  }

  SECTION("mutatePipeline preserves ResultsSummaryPipe statistics across edits") {
    // Regression test for the "best ever resets to 0 when I edit anything" failure
    // mode: mutatePipeline rebuilds pipes from JSON which would wipe accumulated
    // statistics. We migrate state by (name, type) so editing an unrelated knob keeps
    // the rolling window and best-ever intact.
    const uint32_t id = manager.createPipeline("summary-migration-test");
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["summary"] = R"({
        "type": "ResultsSummaryPipe",
        "parameters": { "max_candidates": 4, "window_size": 16 }
      })"_json;
    });
    {
      auto& live = manager.getPipelineById(id);
      auto summary_pipe = std::dynamic_pointer_cast<ResultsSummaryPipe>(
          live.pipeline->getPipes().front()->pipe);
      REQUIRE(summary_pipe != nullptr);
      summary_pipe->addInputWithScore(0, Pipe::OutputItem{{0xAA, 0xBB}, 0.75});
      summary_pipe->addInputWithScore(0, Pipe::OutputItem{{0x01}, 0.30});
      summary_pipe->execute();
      const auto pre = summary_pipe->getSummary();
      REQUIRE(pre.count_total == 2);
      REQUIRE(pre.best_ever_score == Approx(0.75));
      REQUIRE(pre.best_ever_data == std::vector<unsigned char>{0xAA, 0xBB});
    }
    // Edit an unrelated knob (window_size): the new pipe should report the same total
    // count, best-ever score, and best-ever payload despite being a freshly constructed
    // C++ object.
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["summary"]["parameters"]["window_size"] = 32;
    });
    {
      auto& live = manager.getPipelineById(id);
      auto summary_pipe = std::dynamic_pointer_cast<ResultsSummaryPipe>(
          live.pipeline->getPipes().front()->pipe);
      REQUIRE(summary_pipe != nullptr);
      REQUIRE(summary_pipe->getWindowSize() == 32);
      const auto post = summary_pipe->getSummary();
      REQUIRE(post.count_total == 2);
      REQUIRE(post.best_ever_score == Approx(0.75));
      REQUIRE(post.best_ever_data == std::vector<unsigned char>{0xAA, 0xBB});
    }
  }

  SECTION("mutatePipeline rolls back when validation fails") {
    // Regression test: a malformed edit (here, declaring a connection to a non-existent
    // pipe) must leave the live Pipeline and on-disk model unchanged because
    // constructPipelineFromJson throws during the validation step.
    const uint32_t id = manager.createPipeline("rollback-test");
    manager.mutatePipeline(id, [](nlohmann::json& model, nlohmann::json& /*metadata*/) {
      model["pipes"]["sink"] = R"({
        "type": "NullSinkPipe",
        "parameters": { "max_candidates": 1 }
      })"_json;
    });
    const auto pipes_before = manager.getPipelineById(id).pipeline->getPipes().size();
    REQUIRE_THROWS_AS(manager.mutatePipeline(id,
                                             [](nlohmann::json& model,
                                                nlohmann::json& /*metadata*/) {
                                               model["connections"].push_back(R"({
            "source_pipe": "ghost",
            "source_slot": 0,
            "destination_pipe": "sink",
            "destination_slot": 0,
            "buffer_size": 1
          })"_json);
                                             }),
                      std::invalid_argument);
    REQUIRE(manager.getPipelineById(id).pipeline->getPipes().size() == pipes_before);
  }

  // Clean up temporary files
  std::filesystem::remove_all(temp_storage_path);
}

} // namespace beast
