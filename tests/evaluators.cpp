#include <catch2/catch.hpp>

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
