#include <catch2/catch.hpp>

#include <memory>
#include <numeric>

#include <beast/beast.hpp>

TEST_CASE("op_usage_evaluator_returns_correct_fraction_of_noop_operations", "evaluators") {
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

  beast::OperatorUsageEvaluator evaluator(beast::OpCode::NoOp);
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score - 0.25) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("op_usage_evaluator_returns_zero_if_no_operations_present", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::OperatorUsageEvaluator evaluator(beast::OpCode::NoOp);
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("op_usage_evaluator_returns_zero_if_no_noop_operations_present", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);

  beast::OperatorUsageEvaluator evaluator(beast::OpCode::NoOp);
  const double score = evaluator.evaluate(session);

  REQUIRE(std::abs(score) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("op_usage_evaluator_returns_one_if_only_noop_operations_present", "evaluators") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::NoOp);
  session.informAboutStep(beast::OpCode::NoOp);

  beast::OperatorUsageEvaluator evaluator(beast::OpCode::NoOp);
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

  std::shared_ptr<beast::OperatorUsageEvaluator> noop_evaluator_1 =
      std::make_shared<beast::OperatorUsageEvaluator>(beast::OpCode::NoOp);
  std::shared_ptr<beast::OperatorUsageEvaluator> noop_evaluator_2 =
      std::make_shared<beast::OperatorUsageEvaluator>(beast::OpCode::NoOp);

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

TEST_CASE("dyn_focused_rs_eval_yields_zero_score_for_only_noop_programs", "evaluators") {
  beast::Program prg;
  prg.noop();
  prg.noop();
  prg.noop();
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  beast::RuntimeStatisticsEvaluator evaluator(1.0, 0.0);

  REQUIRE(std::abs(evaluator.evaluate(session)) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("stat_focused_rs_eval_yields_one_score_for_only_noop_programs", "evaluators") {
  beast::Program prg;
  prg.noop();
  prg.noop();
  prg.noop();
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  beast::RuntimeStatisticsEvaluator evaluator(0.0, 1.0);

  REQUIRE(std::abs(1.0 - evaluator.evaluate(session)) <
          std::numeric_limits<double>::epsilon());
}

TEST_CASE("both_focused_rs_eval_yields_half_score_for_only_noop_programs", "evaluators") {
  beast::Program prg;
  prg.noop();
  prg.noop();
  prg.noop();
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  beast::RuntimeStatisticsEvaluator evaluator(0.5, 0.5);

  REQUIRE(std::abs(0.5 - evaluator.evaluate(session)) <
          std::numeric_limits<double>::epsilon());
}

TEST_CASE("exec_focused_rs_eval_yields_zero_score_for_compl_exec_prgs", "evaluators") {
  beast::Program prg;
  prg.noop();
  prg.noop();
  prg.noop();
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  beast::RuntimeStatisticsEvaluator evaluator(0.0, 0.0);

  REQUIRE(std::abs(evaluator.evaluate(session)) < std::numeric_limits<double>::epsilon());
}

TEST_CASE("exec_focused_rs_eval_yields_partial_score_for_partially_exec_prgs", "evaluators") {
  beast::Program prg;
  prg.terminate(0);
  prg.noop();
  prg.noop();
  prg.noop();
  beast::VmSession session(std::move(prg), 0, 0, 0);

  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  beast::RuntimeStatisticsEvaluator evaluator(0.0, 0.0);

  REQUIRE(std::abs(0.75 - evaluator.evaluate(session)) <
          std::numeric_limits<double>::epsilon());
}
