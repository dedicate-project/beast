#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("checking_if_stack_is_empty_works", "stacks") {
  const int32_t stack_variable_index_1 = 2;
  const int32_t stack_variable_index_2 = 3;
  const int32_t stack_size_1 = 0;
  const int32_t stack_size_2 = 5;
  const int32_t target_variable_index_1 = 10;
  const int32_t target_variable_index_2 = 11;

  beast::Program prg;
  prg.declareVariable(stack_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(stack_variable_index_1, stack_size_1, true);
  prg.declareVariable(stack_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(stack_variable_index_2, stack_size_2, true);
  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);
  prg.checkIfStackIsEmpty(stack_variable_index_1, true, target_variable_index_1, true);
  prg.checkIfStackIsEmpty(stack_variable_index_2, true, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("stacks_can_push_and_pop_constant_values", "stacks") {
  const int32_t stack_variable_index = 10;
  const int32_t constant_1 = 4;
  const int32_t constant_2 = 128;
  const int32_t dummy_value = 170;
  const int32_t constant_3 = -400198;
  const int32_t variable_index_1 = 3;
  const int32_t variable_index_2 = 27;
  const int32_t variable_index_3 = 100;
  const int32_t variable_index_empty_check = 81;

  beast::Program prg;
  prg.declareVariable(stack_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(stack_variable_index, 0, true);

  prg.declareVariable(stack_variable_index + 1, beast::Program::VariableType::Int32);
  prg.declareVariable(stack_variable_index + 2, beast::Program::VariableType::Int32);
  prg.declareVariable(stack_variable_index + 3, beast::Program::VariableType::Int32);
  prg.declareVariable(stack_variable_index + 4, beast::Program::VariableType::Int32);

  prg.declareVariable(variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_1, 0, true);
  prg.declareVariable(variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_2, 0, true);
  prg.declareVariable(variable_index_3, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_3, 0, true);
  prg.declareVariable(variable_index_empty_check, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_empty_check, 0, true);

  prg.pushConstantOnStack(stack_variable_index, true, constant_3);
  prg.pushConstantOnStack(stack_variable_index, true, dummy_value);
  prg.pushConstantOnStack(stack_variable_index, true, constant_2);
  prg.pushConstantOnStack(stack_variable_index, true, constant_1);

  prg.popVariableFromStack(stack_variable_index, true, variable_index_1, true);
  prg.popVariableFromStack(stack_variable_index, true, variable_index_2, true);
  prg.popTopItemFromStack(stack_variable_index, true);
  prg.popVariableFromStack(stack_variable_index, true, variable_index_3, true);

  prg.checkIfStackIsEmpty(stack_variable_index, true, variable_index_empty_check, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index_1, true) == constant_1);
  REQUIRE(session.getVariableValue(variable_index_2, true) == constant_2);
  REQUIRE(session.getVariableValue(variable_index_3, true) == constant_3);
  REQUIRE(session.getVariableValue(variable_index_empty_check, true) == 0x1);
}

TEST_CASE("stacks_can_push_and_pop_variable_values", "stacks") {
  const int32_t stack_variable_index = 10;
  const int32_t input_variable_index_1 = 50;
  const int32_t input_variable_value_1 = -1;
  const int32_t input_variable_index_2 = 51;
  const int32_t input_variable_value_2 = 1998;
  const int32_t input_variable_index_3 = 52;
  const int32_t input_variable_value_3 = -59678;
  const int32_t variable_index_1 = 4;
  const int32_t variable_index_2 = 28;
  const int32_t variable_index_3 = 101;

  beast::Program prg;
  prg.declareVariable(stack_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(stack_variable_index, 0, true);

  prg.declareVariable(input_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(input_variable_index_1, input_variable_value_1, true);
  prg.declareVariable(input_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(input_variable_index_2, input_variable_value_2, true);
  prg.declareVariable(input_variable_index_3, beast::Program::VariableType::Int32);
  prg.setVariable(input_variable_index_3, input_variable_value_3, true);

  prg.declareVariable(stack_variable_index + 1, beast::Program::VariableType::Int32);
  prg.declareVariable(stack_variable_index + 2, beast::Program::VariableType::Int32);
  prg.declareVariable(stack_variable_index + 3, beast::Program::VariableType::Int32);

  prg.declareVariable(variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_1, 0, true);
  prg.declareVariable(variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_2, 0, true);
  prg.declareVariable(variable_index_3, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_3, 0, true);

  prg.pushVariableOnStack(stack_variable_index, true, input_variable_index_3, true);
  prg.pushVariableOnStack(stack_variable_index, true, input_variable_index_2, true);
  prg.pushVariableOnStack(stack_variable_index, true, input_variable_index_1, true);

  prg.popVariableFromStack(stack_variable_index, true, variable_index_1, true);
  prg.popVariableFromStack(stack_variable_index, true, variable_index_2, true);
  prg.popVariableFromStack(stack_variable_index, true, variable_index_3, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index_1, true) == input_variable_value_1);
  REQUIRE(session.getVariableValue(variable_index_2, true) == input_variable_value_2);
  REQUIRE(session.getVariableValue(variable_index_3, true) == input_variable_value_3);
}
