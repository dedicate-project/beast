#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>
#include <beast/opcodes.hpp>

TEST_CASE("declare_variable", "instructions") {
  const int32_t variable_index = 43;

  beast::Program prg(6);
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::DeclareVariable));
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == 0);
}

TEST_CASE("set_variable", "instructions") {
  const int32_t variable_index = 43;
  const int32_t variable_content = 77612;
  const bool follow_links = false;

  beast::Program prg(10);
  prg.setVariable(variable_index, variable_content, follow_links);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::SetVariable));
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == (follow_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == variable_content);
}

TEST_CASE("undeclare_variable", "instructions") {
  const int32_t variable_index = 43;

  beast::Program prg(6);
  prg.undeclareVariable(variable_index);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::UndeclareVariable));
  REQUIRE(prg.getData4(1) == variable_index);
}

TEST_CASE("add_constant_to_variable", "instructions") {
  const int32_t variable_index = 22;
  const int32_t constant = -91;
  const bool follow_links = true;

  beast::Program prg(10);
  prg.addConstantToVariable(variable_index, constant, follow_links);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::AddConstantToVariable));
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == (follow_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == constant);
}

TEST_CASE("add_variable_to_variable", "instructions") {
  const int32_t source_variable_index = 10;
  const bool follow_source_links = true;
  const int32_t destination_variable_index = 20;
  const bool follow_destination_links = true;

  beast::Program prg(11);
  prg.addVariableToVariable(source_variable_index, follow_source_links, destination_variable_index, follow_destination_links);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::AddVariableToVariable));
  REQUIRE(prg.getData4(1) == source_variable_index);
  REQUIRE(prg.getData1(5) == (follow_source_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == destination_variable_index);
  REQUIRE(prg.getData1(10) == (follow_destination_links ? 0x1 : 0x0));
}

TEST_CASE("set_direct_variable_value", "programs") {
  const int32_t index = 3;
  const int32_t value = 73;

  beast::Program prg(100);
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, value, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index, true) == value);
}

TEST_CASE("set_linked_variable_value", "programs") {
  const int32_t var_index = 3;
  const int32_t var_value = 73;
  const int32_t link_index = 2;

  beast::Program prg(100);
  // Set up the value variable
  prg.declareVariable(var_index, beast::Program::VariableType::Int32);
  prg.setVariable(var_index, var_value, true);
  // Set up the link variable
  prg.declareVariable(link_index, beast::Program::VariableType::Link);
  prg.setVariable(link_index, var_index, false);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(link_index, true) == var_value);
}

TEST_CASE("copying_a_variable_copies_its_value", "programs") {
  const int32_t source_variable_index = 3;
  const int32_t destination_variable_index = 7;
  const int32_t value = 73;

  beast::Program prg(100);
  prg.declareVariable(source_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(source_variable_index, value, true);
  prg.declareVariable(destination_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(destination_variable_index, 0, true);
  prg.copyVariable(source_variable_index, true, destination_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(source_variable_index, true) == session.getVariableValue(destination_variable_index, true));
}

TEST_CASE("undeclared_variables_cannot_be_set", "programs") {
  const int32_t index = 3;
  const int32_t value = 73;

  beast::Program prg;
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.undeclareVariable(index);
  prg.setVariable(index, value, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  vm.step(session);
  vm.step(session);

  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("variables_can_be_swapped", "programs") {
  const int32_t variable_index_a = 0;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_a = 15;
  const int32_t variable_value_b = 189;

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.swapVariables(variable_index_a, true, variable_index_b, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index_a, true) == variable_value_b);
  REQUIRE(session.getVariableValue(variable_index_b, true) == variable_value_a);
}
