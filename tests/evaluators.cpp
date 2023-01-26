#include <catch2/catch.hpp>

#include <memory>
#include <numeric>

#include <beast/beast.hpp>

TEST_CASE("noop_evaluator_returns_correct_fraction_of_noop_operations", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::PerformSystemCall);

  beast::NoOpEvaluator evaluator;
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score - 0.25) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("noop_evaluator_returns_zero_if_no_operations_present", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::NoOpEvaluator evaluator;
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("noop_evaluator_returns_zero_if_no_noop_operations_present", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);

  beast::NoOpEvaluator evaluator;
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("noop_evaluator_returns_one_if_only_noop_operations_present", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::NoOp);

  beast::NoOpEvaluator evaluator;
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score - 1.0) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("aggregation_evaluator_averages_values_for_equal_weights_in_noop_eval", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);

  std::shared_ptr<beast::NoOpEvaluator> noop_evaluator_1 = std::make_shared<beast::NoOpEvaluator>();
  std::shared_ptr<beast::NoOpEvaluator> noop_evaluator_2 = std::make_shared<beast::NoOpEvaluator>();

  beast::AggregationEvaluator evaluator;
  evaluator.addEvaluator(noop_evaluator_1, 1.0, false);
  evaluator.addEvaluator(noop_evaluator_2, 1.0, false);

  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score - 0.5) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("aggregation_evaluator_throws_if_no_evaluator_added", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::AggregationEvaluator evaluator;

  bool threw = false;
  try {
    evaluator.evaluate(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}
