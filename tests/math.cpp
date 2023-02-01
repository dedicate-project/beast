#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("add_constant_to_variable", "math") {
  const int32_t index = 3;
  const int32_t value = 73;
  const int32_t added_constant = 2;

  beast::Program prg(100);
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, value, true);
  prg.addConstantToVariable(index, added_constant, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(index, true) == value + added_constant);
}

TEST_CASE("add_variable_to_variable", "math") {
  const int32_t index_a = 3;
  const int32_t value_a = 73;
  const int32_t index_b = 5;
  const int32_t value_b = 2;

  beast::Program prg(100);
  prg.declareVariable(index_a, beast::Program::VariableType::Int32);
  prg.setVariable(index_a, value_a, true);
  prg.declareVariable(index_b, beast::Program::VariableType::Int32);
  prg.setVariable(index_b, value_b, true);
  prg.addVariableToVariable(index_a, true, index_b, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(index_b, true) == value_a + value_b);
}

TEST_CASE("subtract_constant_from_variable", "math") {
  const int32_t index = 3;
  const int32_t value = 73;
  const int32_t subtracted_constant = 2;

  beast::Program prg(100);
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, value, true);
  prg.subtractConstantFromVariable(index, subtracted_constant, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(index, true) == value - subtracted_constant);
}

TEST_CASE("subtract_variable_from_variable", "math") {
  const int32_t index_a = 3;
  const int32_t value_a = 73;
  const int32_t index_b = 5;
  const int32_t value_b = 2;

  beast::Program prg(100);
  prg.declareVariable(index_a, beast::Program::VariableType::Int32);
  prg.setVariable(index_a, value_a, true);
  prg.declareVariable(index_b, beast::Program::VariableType::Int32);
  prg.setVariable(index_b, value_b, true);
  prg.subtractVariableFromVariable(index_a, true, index_b, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(index_b, true) == value_b - value_a);
}

TEST_CASE("variable_can_be_compared_for_gt_against_constant", "math") {
  const int32_t variable_index = 2;
  const int32_t variable_value = 6;
  const int32_t comparison_value_1 = 3;
  const int32_t comparison_value_2 = 12;
  const int32_t target_variable_index_1 = 10;
  const int32_t target_variable_index_2 = 11;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);
  prg.compareIfVariableGtConstant(variable_index, true, comparison_value_1, target_variable_index_1, true);
  prg.compareIfVariableGtConstant(variable_index, true, comparison_value_2, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("variable_can_be_compared_for_lt_against_constant", "math") {
  const int32_t variable_index = 2;
  const int32_t variable_value = 6;
  const int32_t comparison_value_1 = 3;
  const int32_t comparison_value_2 = 12;
  const int32_t target_variable_index_1 = 10;
  const int32_t target_variable_index_2 = 11;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);
  prg.compareIfVariableLtConstant(variable_index, true, comparison_value_1, target_variable_index_1, true);
  prg.compareIfVariableLtConstant(variable_index, true, comparison_value_2, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x0);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x1);
}

TEST_CASE("variable_can_be_compared_for_eq_against_constant", "math") {
  const int32_t variable_index = 2;
  const int32_t variable_value = 6;
  const int32_t comparison_value_1 = 2;
  const int32_t comparison_value_2 = 6;
  const int32_t target_variable_index_1 = 10;
  const int32_t target_variable_index_2 = 11;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);
  prg.compareIfVariableEqConstant(variable_index, true, comparison_value_1, target_variable_index_1, true);
  prg.compareIfVariableEqConstant(variable_index, true, comparison_value_2, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x0);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x1);
}

TEST_CASE("variable_can_be_compared_for_gt_against_variable", "math") {
  const int32_t variable_index_a = 0;
  const int32_t variable_value_a = 6;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_b = 3;
  const int32_t variable_index_c = 2;
  const int32_t variable_value_c = 12;
  const int32_t target_variable_index_1 = 4;
  const int32_t target_variable_index_2 = 5;

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.declareVariable(variable_index_c, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_c, variable_value_c, true);

  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);

  prg.compareIfVariableGtVariable(variable_index_a, true, variable_index_b, true, target_variable_index_1, true);
  prg.compareIfVariableGtVariable(variable_index_a, true, variable_index_c, true, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("variable_can_be_compared_for_lt_against_variable", "math") {
  const int32_t variable_index_a = 0;
  const int32_t variable_value_a = 6;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_b = 3;
  const int32_t variable_index_c = 2;
  const int32_t variable_value_c = 12;
  const int32_t target_variable_index_1 = 4;
  const int32_t target_variable_index_2 = 5;

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.declareVariable(variable_index_c, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_c, variable_value_c, true);

  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);

  prg.compareIfVariableLtVariable(variable_index_a, true, variable_index_b, true, target_variable_index_1, true);
  prg.compareIfVariableLtVariable(variable_index_a, true, variable_index_c, true, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x0);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x1);
}

TEST_CASE("variable_can_be_compared_for_eq_against_variable", "math") {
  const int32_t variable_index_a = 0;
  const int32_t variable_value_a = 6;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_b = 6;
  const int32_t variable_index_c = 2;
  const int32_t variable_value_c = 12;
  const int32_t target_variable_index_1 = 4;
  const int32_t target_variable_index_2 = 5;

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.declareVariable(variable_index_c, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_c, variable_value_c, true);

  prg.declareVariable(target_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_1, 0, true);
  prg.declareVariable(target_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index_2, 0, true);

  prg.compareIfVariableEqVariable(variable_index_a, true, variable_index_b, true, target_variable_index_1, true);
  prg.compareIfVariableEqVariable(variable_index_a, true, variable_index_c, true, target_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("variables_can_be_moduloed_by_constant", "math") {
  const int32_t variable_index = 0;
  const int32_t variable_value = 59964;
  const int32_t modulo_constant = 27;
  const int32_t expected_result = 24;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.moduloVariableByConstant(variable_index, true, modulo_constant);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == expected_result);
}

TEST_CASE("variables_can_be_moduloed_by_variable", "math") {
  const int32_t variable_index = 0;
  const int32_t variable_value = 59964;
  const int32_t modulo_variable_index = 1;
  const int32_t modulo_variable_value = 28;
  const int32_t expected_result = 16;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.declareVariable(modulo_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(modulo_variable_index, modulo_variable_value, true);
  prg.moduloVariableByVariable(variable_index, true, modulo_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == expected_result);
}

TEST_CASE("max_can_be_acquired_from_variable_and_constant", "math") {
  const int32_t variable_index = 0;
  const int32_t variable_value = 122;
  const int32_t constant = -500;
  const int32_t target_variable_index = 1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.declareVariable(target_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index, 0, true);
  prg.getMaxOfVariableAndConstant(variable_index, true, constant, target_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index, true) == variable_value);
}

TEST_CASE("min_can_be_acquired_from_variable_and_constant", "math") {
  const int32_t variable_index = 0;
  const int32_t variable_value = 122;
  const int32_t constant = -500;
  const int32_t target_variable_index = 1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, variable_value, true);
  prg.declareVariable(target_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index, 0, true);
  prg.getMinOfVariableAndConstant(variable_index, true, constant, target_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index, true) == constant);
}

TEST_CASE("max_can_be_acquired_from_variable_and_variable", "math") {
  const int32_t variable_index_a = 0;
  const int32_t variable_value_a = 122;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_b = 619;
  const int32_t target_variable_index = 2;

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.declareVariable(target_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index, 0, true);
  prg.getMaxOfVariableAndVariable(variable_index_a, true, variable_index_b, true, target_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index, true) == variable_value_b);
}

TEST_CASE("min_can_be_acquired_from_variable_and_variable", "math") {
  const int32_t variable_index_a = 0;
  const int32_t variable_value_a = 122;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_b = 619;
  const int32_t target_variable_index = 2;

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.declareVariable(target_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(target_variable_index, 0, true);
  prg.getMinOfVariableAndVariable(variable_index_a, true, variable_index_b, true, target_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session, false)) {}

  REQUIRE(session.getVariableValue(target_variable_index, true) == variable_value_a);
}
