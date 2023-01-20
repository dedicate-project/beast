#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("terminate_loop_while_variable_gt_0_with_variable_jump_address", "jumps") {
  beast::Program prg(150);
  // Counting variable
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 3, true);
  // Validation variable
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  // Jump address variable
  prg.declareVariable(2, beast::Program::VariableType::Int32);

  const int32_t loop_start_address = prg.getPointer();
  prg.setVariable(2, loop_start_address, true);

  // Main loop
  prg.subtractConstantFromVariable(0, 1, true);  // Decrease counting var by 1
  prg.addConstantToVariable(1, 2, true);  // Increase validation var by 2

  // Jump back to loop start if var 0 > 0.
  prg.absoluteJumpToVariableAddressIfVariableGreaterThanZero(0, true, 2, true);
  // If we pass this, counting var 0 should be 0 (loop iterated 3 times). That means, validation
  // variable 1 should be 6.

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(1, true) == 6);
}

TEST_CASE("terminate_loop_while_variable_lt_0_with_variable_jump_address", "jumps") {
  beast::Program prg(150);
  // Counting variable
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, -4, true);
  // Validation variable
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  // Jump address variable
  prg.declareVariable(2, beast::Program::VariableType::Int32);

  const int32_t loop_start_address = prg.getPointer();
  prg.setVariable(2, loop_start_address, true);

  // Main loop
  prg.addConstantToVariable(0, 1, true);  // Increase counting var by 1
  prg.addConstantToVariable(1, 2, true);  // Increase validation var by 2

  // Jump back to loop start if var 0 > 0.
  prg.absoluteJumpToVariableAddressIfVariableLessThanZero(0, true, 2, true);
  // If we pass this, counting var 0 should be 0 (loop iterated 4 times). That means, validation
  // variable 1 should be 8.

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(1, true) == 8);
}

TEST_CASE("terminate_loop_while_variable_eq_0_with_variable_jump_address", "jumps") {
  beast::Program prg(150);
  // Counting variable
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, -1, true);
  // Validation variable
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  // Jump address variable
  prg.declareVariable(2, beast::Program::VariableType::Int32);

  const int32_t loop_start_address = prg.getPointer();
  prg.setVariable(2, loop_start_address, true);

  // Main loop
  prg.addConstantToVariable(0, 1, true);  // Increase counting var by 1
  prg.addConstantToVariable(1, 2, true);  // Increase validation var by 2

  // Jump back to loop start if var 0 > 0.
  prg.absoluteJumpToVariableAddressIfVariableEqualsZero(0, true, 2, true);
  // If we pass this, counting var 0 should be 0 (loop iterated 2 times). That means, validation
  // variable 1 should be 4.

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(1, true) == 4);
}

TEST_CASE("terminate_loop_while_variable_gt_0_with_fixed_jump_address", "jumps") {
  beast::Program prg(150);
  // Counting variable
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 3, true);
  // Validation variable
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);

  const int32_t loop_start_address = prg.getPointer();

  // Main loop
  prg.subtractConstantFromVariable(0, 1, true);  // Decrease counting var by 1
  prg.addConstantToVariable(1, 2, true);  // Increase validation var by 2

  // Jump back to loop start if var 0 > 0.
  prg.absoluteJumpToAddressIfVariableGreaterThanZero(0, true, loop_start_address);
  // If we pass this, counting var 0 should be 0 (loop iterated 3 times). That means, validation
  // variable 1 should be 6.

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(1, true) == 6);
}

TEST_CASE("terminate_loop_while_variable_lt_0_with_fixed_jump_address", "jumps") {
  beast::Program prg(150);
  // Counting variable
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, -4, true);
  // Validation variable
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);

  const int32_t loop_start_address = prg.getPointer();

  // Main loop
  prg.addConstantToVariable(0, 1, true);  // Increase counting var by 1
  prg.addConstantToVariable(1, 2, true);  // Increase validation var by 2

  // Jump back to loop start if var 0 > 0.
  prg.absoluteJumpToAddressIfVariableLessThanZero(0, true, loop_start_address);
  // If we pass this, counting var 0 should be 0 (loop iterated 4 times). That means, validation
  // variable 1 should be 8.

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(1, true) == 8);
}

TEST_CASE("terminate_loop_while_variable_eq_0_with_fixed_jump_address", "jumps") {
  beast::Program prg(150);
  // Counting variable
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, -1, true);
  // Validation variable
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);

  const int32_t loop_start_address = prg.getPointer();

  // Main loop
  prg.addConstantToVariable(0, 1, true);  // Increase counting var by 1
  prg.addConstantToVariable(1, 2, true);  // Increase validation var by 2

  // Jump back to loop start if var 0 > 0.
  prg.absoluteJumpToAddressIfVariableEqualsZero(0, true, loop_start_address);
  // If we pass this, counting var 0 should be 0 (loop iterated 2 times). That means, validation
  // variable 1 should be 4.

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(1, true) == 4);
}

TEST_CASE("unconditional_jump_to_absolute_address_works", "jumps") {
  beast::Program prg;
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  const int32_t pointer = prg.getPointer();
  prg.unconditionalJumpToAbsoluteAddress(pointer + 5 + 10);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
}

TEST_CASE("unconditional_jump_to_absolute_variable_address_works", "jumps") {
  beast::Program prg;
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 6 + 10 + 6 + 10 + 5 + 10, true);
  prg.unconditionalJumpToAbsoluteVariableAddress(1, true);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
}

TEST_CASE("unconditional_jump_to_relative_address_works", "jumps") {
  beast::Program prg;
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.unconditionalJumpToRelativeAddress(5 + 10);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
}

TEST_CASE("unconditional_jump_to_relative_variable_address_works", "jumps") {
  beast::Program prg;
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 5 + 10, true);
  prg.unconditionalJumpToRelativeVariableAddress(1, true);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
}
