#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("inputs_can_be_determined", "programs") {
  const int32_t input_variable_index = 42;
  const int32_t result1_variable_index = 11;
  const int32_t regular_variable_index = 25;
  const int32_t result2_variable_index = 30;

  beast::Program prg(100);
  prg.declareVariable(result1_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(result1_variable_index, 0, true);
  prg.declareVariable(result2_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(result2_variable_index, 0, true);
  prg.declareVariable(regular_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(regular_variable_index, 0, true);
  prg.checkIfVariableIsInput(input_variable_index, true, result1_variable_index, true);
  prg.checkIfVariableIsInput(regular_variable_index, true, result2_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(input_variable_index, beast::VmSession::VariableIoBehavior::Input);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(result1_variable_index, true) == 0x1);
  REQUIRE(session.getVariableValue(result2_variable_index, true) == 0x0);
}

TEST_CASE("outputs_can_be_determined", "programs") {
  const int32_t output_variable_index = 29;
  const int32_t result1_variable_index = 110;
  const int32_t regular_variable_index = 2;
  const int32_t result2_variable_index = 80;

  beast::Program prg(100);
  prg.declareVariable(result1_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(result1_variable_index, 0, true);
  prg.declareVariable(result2_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(result2_variable_index, 0, true);
  prg.declareVariable(regular_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(regular_variable_index, 0, true);
  prg.checkIfVariableIsOutput(output_variable_index, true, result1_variable_index, true);
  prg.checkIfVariableIsOutput(regular_variable_index, true, result2_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(output_variable_index, beast::VmSession::VariableIoBehavior::Output);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(result1_variable_index, true) == 0x1);
  REQUIRE(session.getVariableValue(result2_variable_index, true) == 0x0);
}

TEST_CASE("input_count_can_be_determined", "programs") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.loadInputCountIntoVariable(0, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(3, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(5, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(8, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(62, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(120, beast::VmSession::VariableIoBehavior::Input);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 2);
}

TEST_CASE("output_count_can_be_determined", "programs") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.loadOutputCountIntoVariable(0, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(3, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(5, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(8, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(62, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(120, beast::VmSession::VariableIoBehavior::Input);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 3);
}

TEST_CASE("set_input_can_be_determined", "programs") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 1, true);
  prg.checkIfInputWasSet(2, true, 0, true);
  prg.checkIfInputWasSet(3, true, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(2, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(3, beast::VmSession::VariableIoBehavior::Input);

  session.setVariableValue(2, true, 1);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 1);
  REQUIRE(session.getVariableValue(1, true) == 0);
}
