#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("variables_can_be_variably_bit_shifted_left", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 2;
  const int32_t shifted_value = 16;
  const int8_t shift_amount = 3;
  const int32_t amount_variable_index = 3;

  beast::Program prg;
  prg.declareVariable(amount_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(amount_variable_index, shift_amount, true);
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.variableBitShiftVariableLeft(variable_index, true, amount_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == shifted_value);
}

TEST_CASE("variables_can_be_variably_bit_shifted_right", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 16;
  const int32_t shifted_value = 2;
  const int8_t shift_amount = 3;
  const int32_t amount_variable_index = 3;

  beast::Program prg;
  prg.declareVariable(amount_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(amount_variable_index, shift_amount, true);
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.variableBitShiftVariableRight(variable_index, true, amount_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == shifted_value);
}


TEST_CASE("variables_can_be_rotated_left", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 0x00499602;
  const int32_t rotated_value = 0x96020049;
  const int8_t rotate_amount = 16;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.rotateVariableLeft(variable_index, true, rotate_amount);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == rotated_value);
}

TEST_CASE("variables_can_be_rotated_right", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 0x00499602;
  const int32_t rotated_value = 0x02004996;
  const int8_t rotate_amount = 8;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.rotateVariableRight(variable_index, true, rotate_amount);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == rotated_value);
}

TEST_CASE("variables_can_be_rotated_left_by_variable_places", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 0x00499602;
  const int32_t rotated_value = 0x96020049;
  const int8_t rotate_amount = 16;
  const int32_t places_variable_index = 1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.declareVariable(places_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(places_variable_index, rotate_amount, true);
  prg.variableRotateVariableLeft(variable_index, true, places_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == rotated_value);
}

TEST_CASE("variables_can_be_rotated_right_by_variable_places", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 0x00499602;
  const int32_t rotated_value = 0x02004996;
  const int8_t rotate_amount = 8;
  const int32_t places_variable_index = 1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.declareVariable(places_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(places_variable_index, rotate_amount, true);
  prg.variableRotateVariableRight(variable_index, true, places_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == rotated_value);
}

TEST_CASE("variables_can_be_bit_wise_inverted", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 958208765;
  const int32_t inverted_value = -958208766;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.bitWiseInvertVariable(variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == inverted_value);
}

TEST_CASE("variables_can_be_bit_wise_anded", "bit_manipulation") {
  const int32_t variable_index_a = 0;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_a = 52766103;
  const int32_t variable_value_b = 99021920;
  const auto expected_result =
      static_cast<int32_t>(
          static_cast<uint32_t>(variable_value_a) & static_cast<uint32_t>(variable_value_b));

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.bitWiseAndTwoVariables(variable_index_a, true, variable_index_b, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index_b, true) == expected_result);
}

TEST_CASE("variables_can_be_bit_wise_ored", "bit_manipulation") {
  const int32_t variable_index_a = 0;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_a = 52766103;
  const int32_t variable_value_b = 99021920;
  const auto expected_result =
      static_cast<int32_t>(
          static_cast<uint32_t>(variable_value_a) | static_cast<uint32_t>(variable_value_b));

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.bitWiseOrTwoVariables(variable_index_a, true, variable_index_b, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index_b, true) == expected_result);
}

TEST_CASE("variables_can_be_bit_wise_xored", "bit_manipulation") {
  const int32_t variable_index_a = 0;
  const int32_t variable_index_b = 1;
  const int32_t variable_value_a = 52766103;
  const int32_t variable_value_b = 99021920;
  const auto expected_result =
      static_cast<int32_t>(
          static_cast<uint32_t>(variable_value_a) ^ static_cast<uint32_t>(variable_value_b));

  beast::Program prg;
  prg.declareVariable(variable_index_a, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_a, variable_value_a, true);
  prg.declareVariable(variable_index_b, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_b, variable_value_b, true);
  prg.bitWiseXorTwoVariables(variable_index_a, true, variable_index_b, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index_b, true) == expected_result);
}

TEST_CASE("variables_can_be_bit_shifted_left", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 2;
  const int32_t shifted_value = 16;
  const int8_t shift_amount = 3;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.bitShiftVariableLeft(variable_index, true, shift_amount);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == shifted_value);
}

TEST_CASE("variables_can_be_bit_shifted_left_by_negative_amount", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 16;
  const int32_t shifted_value = 2;
  const int8_t shift_amount = -3;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.bitShiftVariableLeft(variable_index, true, shift_amount);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == shifted_value);
}

TEST_CASE("variables_can_be_bit_shifted_right", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 16;
  const int32_t shifted_value = 2;
  const int8_t shift_amount = 3;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.bitShiftVariableRight(variable_index, true, shift_amount);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == shifted_value);
}

TEST_CASE("variables_can_be_bit_shifted_right_by_negative_amount", "bit_manipulation") {
  const int32_t variable_index = 0;
  const int32_t nominal_value = 2;
  const int32_t shifted_value = 16;
  const int8_t shift_amount = -3;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, nominal_value, true);
  prg.bitShiftVariableRight(variable_index, true, shift_amount);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == shifted_value);
}
