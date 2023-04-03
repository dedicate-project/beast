#include <catch2/catch.hpp>

// Standard
#include <algorithm>
#include <numeric>
#include <vector>

// Internal
#include <beast/beast.hpp>

TEST_CASE("bubblesort_correctly_sorts_10_numbers", "programs") {
  const std::vector<int32_t> unsorted = {7, 1, 199, -44, 2356, -881, 0, 406, 1, 9};
  const std::vector<int32_t> expected = {-881, -44, 0, 1, 1, 7, 9, 199, 406, 2356};

  const auto numbers = static_cast<int32_t>(unsorted.size());
  REQUIRE(numbers == static_cast<int32_t>(expected.size()));

  std::vector<int32_t> input_variables(numbers);
  std::vector<int32_t> output_variables(numbers);
  std::iota(input_variables.begin(), input_variables.end(), 0);
  std::iota(output_variables.begin(), output_variables.end(), numbers);

  /* Define bookkeeping variables for the algorithm's loops and operations. */
  const int32_t var_i = 2 * numbers;
  const int32_t var_j = 2 * numbers + 1;
  const int32_t var_temp = 2 * numbers + 2;
  const int32_t var_l1 = 2 * numbers + 3;
  const int32_t var_l2 = 2 * numbers + 4;

  /* Define the actual program. First, declare the algorithm's working
   * variables. */
  beast::Program prg;
  prg.declareVariable(var_i, beast::Program::VariableType::Int32);
  prg.declareVariable(var_j, beast::Program::VariableType::Int32);
  prg.declareVariable(var_temp, beast::Program::VariableType::Int32);
  prg.declareVariable(var_l1, beast::Program::VariableType::Link);
  prg.declareVariable(var_l2, beast::Program::VariableType::Link);

  /* Bubblesort uses two cascaded loops; store their starting addresses here to
     be able to conditionally jump back. */
  prg.setVariable(var_i, 0x0, false);
  const auto outer_loop_start = static_cast<int32_t>(prg.getPointer());
  prg.setVariable(var_j, 0x0, false);
  const auto inner_loop_start = static_cast<int32_t>(prg.getPointer());

  /* Here we use the Link variable type for an indirection operation. `var_l1`
     and `var_l2` are links, so their value is used as a variable address when
     reading/writing to them. This way, we can access variables by knowing their
     index in another variable, allowing array iterations and alike. For
     bubblesort, we need this to iterate through all known numbers of the array
     to sort. */
  prg.copyVariable(var_j, true, var_l1, false);
  prg.copyVariable(var_j, true, var_l2, false);
  prg.addConstantToVariable(var_l2, 1, false);
  prg.compareIfVariableGtVariable(var_l1, true, var_l2, true, var_temp, true);
  beast::Program swap;
  swap.swapVariables(var_l1, true, var_l2, true);
  prg.relativeJumpToAddressIfVariableEqualsZero(
      var_temp, true, static_cast<int32_t>(swap.getSize()));
  prg.insertProgram(swap);

  /* The next two blocks are the cascaded loop tails; `var_j` controls the inner
     loop, `var_i` controls the outer loop. */
  prg.addConstantToVariable(var_j, 1, false);
  prg.compareIfVariableLtConstant(var_j, false, numbers - 1, var_temp, true);
  prg.absoluteJumpToAddressIfVariableGreaterThanZero(var_temp, true, inner_loop_start);

  prg.addConstantToVariable(var_i, 1, false);
  prg.compareIfVariableLtConstant(var_i, false, numbers - 1, var_temp, true);
  prg.absoluteJumpToAddressIfVariableGreaterThanZero(var_temp, true, outer_loop_start);

  /* Copy working variables to output after execution. */
  for (int32_t idx : output_variables) {
    prg.copyVariable(idx - numbers, true, idx, true);
  }

  /* Define the session to use, and register input and output variables. */
  beast::VmSession session(std::move(prg), 2 * numbers + 5, 0, 0);
  for (int32_t idx : input_variables) {
    session.setVariableBehavior(idx, beast::VmSession::VariableIoBehavior::Input);
    session.setVariableValue(idx, true, unsorted[idx]);
  }
  for (int32_t idx : output_variables) {
    session.setVariableBehavior(idx, beast::VmSession::VariableIoBehavior::Output);
  }

  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {
  }

  for (int32_t idx : output_variables) {
    REQUIRE(session.getVariableValue(idx, true) == expected[idx - numbers]);
  }
}

TEST_CASE("static_and_dynamic_operator_counts_yield_correct_result", "programs") {
  const std::string message = "Some message.";

  beast::Program prg;
  prg.noop();
  prg.setStringTableEntry(0, message);
  prg.printStringFromStringTable(0);
  prg.terminate(0);
  prg.noop();

  beast::CpuVirtualMachine virtual_machine;

  beast::VmSession session_static(prg, 0, 0, 0);
  while (virtual_machine.step(session_static, true)) {
  }
  beast::VmSession::RuntimeStatistics static_analysis_data = session_static.getRuntimeStatistics();

  beast::VmSession session_dynamic(prg, 0, 1, 50);
  while (virtual_machine.step(session_dynamic, false)) {
  }
  beast::VmSession::RuntimeStatistics dynamic_analysis_data =
      session_dynamic.getRuntimeStatistics();

  beast::OperatorUsageEvaluator noop_evaluator(beast::OpCode::NoOp);
  const double static_noop_ratio = noop_evaluator.evaluate(session_static);
  const double dynamic_noop_ratio = noop_evaluator.evaluate(session_dynamic);

  REQUIRE(std::abs(static_noop_ratio - 0.4) < std::numeric_limits<double>::epsilon());
  REQUIRE(std::abs(dynamic_noop_ratio - 0.25) < std::numeric_limits<double>::epsilon());
}
