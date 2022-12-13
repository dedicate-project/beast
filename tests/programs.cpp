#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("print_from_string_table", "programs") {
  const std::string output = "Output";

  beast::Program prg(100);
  prg.setStringTableEntry(0, output);
  prg.printStringFromStringTable(0);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("print_chars_from_variables", "programs") {
  const std::string output = "Hello";

  beast::Program prg(150);
  prg.declareVariable(10, beast::Program::VariableType::Int32);
  prg.setVariable(10, 0x48, true);  // H
  prg.declareVariable(11, beast::Program::VariableType::Int32);
  prg.setVariable(11, 0x65, true);  // e
  prg.declareVariable(12, beast::Program::VariableType::Int32);
  prg.setVariable(12, 0x6c, true);  // l
  prg.declareVariable(13, beast::Program::VariableType::Int32);
  prg.setVariable(13, 0x6c, true);  // l
  prg.declareVariable(14, beast::Program::VariableType::Int32);
  prg.setVariable(14, 0x6f, true);  // o
  prg.printVariable(10, true, true);
  prg.printVariable(11, true, true);
  prg.printVariable(12, true, true);
  prg.printVariable(13, true, true);
  prg.printVariable(14, true, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
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

TEST_CASE("add_constant_to_variable", "programs") {
  const int32_t index = 3;
  const int32_t value = 73;
  const int32_t added_constant = 2;

  beast::Program prg(100);
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, value, true);
  prg.addConstantToVariable(index, added_constant, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index, true) == value + added_constant);
}

TEST_CASE("add_variable_to_variable", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index_b, true) == value_a + value_b);
}

TEST_CASE("subtract_constant_from_variable", "programs") {
  const int32_t index = 3;
  const int32_t value = 73;
  const int32_t subtracted_constant = 2;

  beast::Program prg(100);
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, value, true);
  prg.subtractConstantFromVariable(index, subtracted_constant, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index, true) == value - subtracted_constant);
}

TEST_CASE("subtract_variable_from_variable", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index_b, true) == value_b - value_a);
}

TEST_CASE("print_conditionally_if_var_not_gt_0", "programs") {
  const std::string output = "Output";
  const std::string checkpoint = "Checkpoint";

  beast::Program prg(150);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, -1, true);
  prg.declareVariable(2, beast::Program::VariableType::Int32);
  prg.setVariable(2, 18, true);
  prg.relativeJumpToVariableAddressIfVariableGreaterThanZero(1, true, 2, true);
  prg.setStringTableEntry(0, output);
  prg.printStringFromStringTable(0);

  prg.declareVariable(3, beast::Program::VariableType::Int32);
  prg.setVariable(3, 1, true);
  prg.declareVariable(4, beast::Program::VariableType::Int32);
  prg.setVariable(4, 22, true);
  prg.relativeJumpToVariableAddressIfVariableGreaterThanZero(3, true, 4, true);
  prg.setStringTableEntry(1, checkpoint);
  prg.printStringFromStringTable(1);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("print_conditionally_if_var_not_lt_0", "programs") {
  const std::string output = "Output";
  const std::string checkpoint = "Checkpoint";

  beast::Program prg(150);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, -1, true);
  prg.declareVariable(2, beast::Program::VariableType::Int32);
  prg.setVariable(2, 22, true);
  prg.relativeJumpToVariableAddressIfVariableLessThanZero(1, true, 2, true);
  prg.setStringTableEntry(0, checkpoint);
  prg.printStringFromStringTable(0);

  prg.declareVariable(3, beast::Program::VariableType::Int32);
  prg.setVariable(3, 1, true);
  prg.declareVariable(4, beast::Program::VariableType::Int32);
  prg.setVariable(4, 18, true);
  prg.relativeJumpToVariableAddressIfVariableLessThanZero(3, true, 4, true);
  prg.setStringTableEntry(1, output);
  prg.printStringFromStringTable(1);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("print_conditionally_if_var_not_eq_0", "programs") {
  const std::string output = "Output";
  const std::string checkpoint = "Checkpoint";

  beast::Program prg(150);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  prg.declareVariable(2, beast::Program::VariableType::Int32);
  prg.setVariable(2, 22, true);
  prg.relativeJumpToVariableAddressIfVariableEqualsZero(1, true, 2, true);
  prg.setStringTableEntry(0, checkpoint);
  prg.printStringFromStringTable(0);

  prg.declareVariable(3, beast::Program::VariableType::Int32);
  prg.setVariable(3, 1, true);
  prg.declareVariable(4, beast::Program::VariableType::Int32);
  prg.setVariable(4, 18, true);
  prg.relativeJumpToVariableAddressIfVariableEqualsZero(3, true, 4, true);
  prg.setStringTableEntry(1, output);
  prg.printStringFromStringTable(1);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("terminate_loop_while_variable_gt_0_with_variable_jump_address", "programs") {
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

TEST_CASE("terminate_loop_while_variable_lt_0_with_variable_jump_address", "programs") {
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

TEST_CASE("terminate_loop_while_variable_eq_0_with_variable_jump_address", "programs") {
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

TEST_CASE("print_conditionally_if_var_gt_0_with_fixed_rel_addr", "programs") {
  const std::string output = "Output";
  const std::string checkpoint = "Checkpoint";

  beast::Program prg(150);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 1, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  // Jump the next two instructions only if variable 0 > 0
  prg.relativeJumpToAddressIfVariableGreaterThanZero(0, true, 22);
  prg.setStringTableEntry(0, checkpoint);
  prg.printStringFromStringTable(0);

  // Jump the next two instructions only if variable 1 > 0
  prg.relativeJumpToAddressIfVariableGreaterThanZero(1, true, 18);
  prg.setStringTableEntry(1, output);
  prg.printStringFromStringTable(1);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("print_conditionally_if_var_lt_0_with_fixed_rel_addr", "programs") {
  const std::string output = "Output";
  const std::string checkpoint = "Checkpoint";

  beast::Program prg(150);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, -1, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  // Jump the next two instructions only if variable 0 < 0
  prg.relativeJumpToAddressIfVariableLessThanZero(0, true, 22);
  prg.setStringTableEntry(0, checkpoint);
  prg.printStringFromStringTable(0);

  // Jump the next two instructions only if variable 1 < 0
  prg.relativeJumpToAddressIfVariableLessThanZero(1, true, 18);
  prg.setStringTableEntry(1, output);
  prg.printStringFromStringTable(1);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("print_conditionally_if_var_eq_0_with_fixed_rel_addr", "programs") {
  const std::string output = "Output";
  const std::string checkpoint = "Checkpoint";

  beast::Program prg(150);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 1, true);
  // Jump the next two instructions only if variable 0 == 0
  prg.relativeJumpToAddressIfVariableEqualsZero(0, true, 22);
  prg.setStringTableEntry(0, checkpoint);
  prg.printStringFromStringTable(0);

  // Jump the next two instructions only if variable 1 == 0
  prg.relativeJumpToAddressIfVariableEqualsZero(1, true, 18);
  prg.setStringTableEntry(1, output);
  prg.printStringFromStringTable(1);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}
