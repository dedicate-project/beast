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

TEST_CASE("terminate_loop_while_variable_gt_0_with_fixed_jump_address", "programs") {
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

TEST_CASE("terminate_loop_while_variable_lt_0_with_fixed_jump_address", "programs") {
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

TEST_CASE("terminate_loop_while_variable_eq_0_with_fixed_jump_address", "programs") {
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

TEST_CASE("store_variable_memory_size_into_variable", "programs") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.loadMemorySizeIntoVariable(0, true);

  beast::VmSession session(std::move(prg), 128, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 128);
}

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

TEST_CASE("current_address_can_be_determined", "programs") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.loadCurrentAddressIntoVariable(0, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 22);
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

TEST_CASE("termination_prevents_further_execution_and_sets_return_code", "programs") {
  const int8_t return_code = 127;
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.terminate(return_code);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
  REQUIRE(session.getReturnCode() == return_code);
}

TEST_CASE("inserted_programs_work_as_intended", "programs") {
  const int32_t index = 3;
  const int32_t value1 = 73;
  const int32_t value2 = 62;

  beast::Program prg1;
  prg1.declareVariable(index, beast::Program::VariableType::Int32);
  prg1.setVariable(index, value1, true);

  beast::Program prg2;
  prg2.setVariable(index, value2, true);

  beast::Program prg3;
  prg3.insertProgram(prg1);
  prg3.insertProgram(prg2);

  beast::VmSession session(std::move(prg3), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index, true) == value2);
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

TEST_CASE("string_table_limit_can_be_determined", "programs") {
  const int32_t index = 7;
  const int32_t string_table_limit = 25;

  beast::Program prg;
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, 0, true);
  prg.loadStringTableLimitIntoVariable(index, true);

  beast::VmSession session(std::move(prg), 500, string_table_limit, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index, true) == string_table_limit);
}

TEST_CASE("string_table_item_length_limit_can_be_determined", "programs") {
  const int32_t index = 7;
  const int32_t string_table_item_length_limit = 17;

  beast::Program prg;
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, 0, true);
  prg.loadStringTableItemLengthLimitIntoVariable(index, true);

  beast::VmSession session(std::move(prg), 500, 100, string_table_item_length_limit);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(index, true) == string_table_item_length_limit);
}
