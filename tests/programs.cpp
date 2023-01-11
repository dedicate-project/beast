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

TEST_CASE("load_random_value_into_variable", "programs") {
  const int32_t index = 2;

  // Assuming that at least one of 10 random values is != 0.

  beast::Program prg;
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, 0, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  vm.step(session);
  vm.step(session);

  bool any_is_random = false;
  while (vm.step(session)) {
    vm.step(session);
    if (session.getVariableValue(index, true) != 0) {
      any_is_random = true;
      break;
    }
  }

  REQUIRE(any_is_random == true);
}

TEST_CASE("unconditional_jump_to_absolute_address_works", "programs") {
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

TEST_CASE("unconditional_jump_to_absolute_variable_address_works", "programs") {
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

TEST_CASE("unconditional_jump_to_relative_address_works", "programs") {
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

TEST_CASE("unconditional_jump_to_relative_variable_address_works", "programs") {
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

TEST_CASE("string_table_item_length_can_be_determined", "programs") {
  const std::string entry1 = "Entry";
  const std::string entry2 = "Another entry";

  beast::Program prg;
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.declareVariable(1, beast::Program::VariableType::Int32);
  prg.setVariable(1, 0, true);
  prg.setStringTableEntry(0, entry1);
  prg.setStringTableEntry(1, entry2);

  prg.loadStringItemLengthIntoVariable(0, 0, true);
  prg.loadStringItemLengthIntoVariable(1, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == entry1.size());
  REQUIRE(session.getVariableValue(1, true) == entry2.size());
}

TEST_CASE("string_table_item_can_be_loaded_into_variables", "programs") {
  const std::string entry = "Entry";

  beast::Program prg;
  for (uint32_t idx = 0; idx < entry.size(); ++idx) {
    prg.declareVariable(idx, beast::Program::VariableType::Int32);
    prg.setVariable(idx, 0, true);
  }
  prg.setStringTableEntry(0, entry);
  prg.loadStringItemIntoVariables(0, 0, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  for (uint32_t idx = 0; idx < entry.size(); ++idx) {
    const int32_t value = session.getVariableValue(idx, true);
    REQUIRE(static_cast<int32_t>(entry.at(idx)) == value);
  }
}

TEST_CASE("variables_can_be_bit_shifted_left", "programs") {
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

TEST_CASE("variables_can_be_bit_shifted_left_by_negative_amount", "programs") {
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

TEST_CASE("variables_can_be_bit_shifted_right", "programs") {
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

TEST_CASE("variables_can_be_bit_shifted_right_by_negative_amount", "programs") {
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

TEST_CASE("variables_can_be_bit_wise_inverted", "programs") {
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

TEST_CASE("variables_can_be_bit_wise_anded", "programs") {
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

TEST_CASE("variables_can_be_bit_wise_ored", "programs") {
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

TEST_CASE("variables_can_be_bit_wise_xored", "programs") {
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

TEST_CASE("string_table_item_can_be_set_at_variable_index", "programs") {
  const std::string entry = "Entry";

  beast::Program prg;
  for (uint32_t idx = 0; idx < entry.size(); ++idx) {
    prg.declareVariable(idx + 1, beast::Program::VariableType::Int32);
    prg.setVariable(idx + 1, 0, true);
  }

  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);

  prg.setVariableStringTableEntry(0, true, entry);
  prg.loadStringItemIntoVariables(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  for (uint32_t idx = 0; idx < entry.size(); ++idx) {
    const int32_t value = session.getVariableValue(idx + 1, true);
    REQUIRE(static_cast<int32_t>(entry.at(idx)) == value);
  }
}

TEST_CASE("variables_can_be_moduloed_by_constant", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == expected_result);
}

TEST_CASE("variables_can_be_moduloed_by_variable", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(variable_index, true) == expected_result);
}

TEST_CASE("print_variable_index_from_string_table", "programs") {
  const int32_t variable_index = 3;
  const int32_t string_table_index = 22;
  const std::string output = "Output";

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, string_table_index, true);
  prg.setStringTableEntry(string_table_index, output);
  prg.printVariableStringFromStringTable(variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("variables_can_be_rotated_left", "programs") {
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

TEST_CASE("variables_can_be_rotated_right", "programs") {
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

TEST_CASE("variables_can_be_rotated_left_by_variable_places", "programs") {
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

TEST_CASE("variables_can_be_rotated_right_by_variable_places", "programs") {
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

TEST_CASE("variable_string_table_item_length_can_be_determined", "programs") {
  const std::string entry1 = "Entry";
  const std::string entry2 = "Another entry";
  const int32_t string_table_index_variable_index_1 = 22;
  const int32_t string_table_index_variable_index_2 = 34;
  const int32_t string_table_index_1 = 12;
  const int32_t string_table_index_2 = 19;
  const int32_t storage_variable_index_1 = 4;
  const int32_t storage_variable_index_2 = 9;

  beast::Program prg;
  prg.declareVariable(storage_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(storage_variable_index_1, 0, true);
  prg.declareVariable(storage_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(storage_variable_index_2, 0, true);
  prg.declareVariable(string_table_index_variable_index_1, beast::Program::VariableType::Int32);
  prg.setVariable(string_table_index_variable_index_1, string_table_index_1, true);
  prg.declareVariable(string_table_index_variable_index_2, beast::Program::VariableType::Int32);
  prg.setVariable(string_table_index_variable_index_2, string_table_index_2, true);
  prg.setStringTableEntry(string_table_index_1, entry1);
  prg.setStringTableEntry(string_table_index_2, entry2);

  prg.loadVariableStringItemLengthIntoVariable(string_table_index_variable_index_1, true, storage_variable_index_1, true);
  prg.loadVariableStringItemLengthIntoVariable(string_table_index_variable_index_2, true, storage_variable_index_2, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(storage_variable_index_1, true) == entry1.size());
  REQUIRE(session.getVariableValue(storage_variable_index_2, true) == entry2.size());
}

TEST_CASE("variable_string_table_item_can_be_loaded_into_variables", "programs") {
  const std::string entry = "Entry";
  const int32_t string_table_variable_index = 21;
  const int32_t string_table_index = 12;
  const int32_t start_variable_index = 5;

  beast::Program prg;
  prg.declareVariable(string_table_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(string_table_variable_index, string_table_index, true);
  for (uint32_t idx = 0; idx < entry.size(); ++idx) {
    prg.declareVariable(start_variable_index + idx, beast::Program::VariableType::Int32);
    prg.setVariable(start_variable_index + idx, 0, true);
  }
  prg.setStringTableEntry(string_table_index, entry);
  prg.loadVariableStringItemIntoVariables(string_table_variable_index, true, start_variable_index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  for (uint32_t idx = 0; idx < entry.size(); ++idx) {
    const int32_t value = session.getVariableValue(start_variable_index + idx, true);
    REQUIRE(static_cast<int32_t>(entry.at(idx)) == value);
  }
}

TEST_CASE("termination_prevents_further_execution_and_sets_variable_return_code", "programs") {
  const int32_t return_code_variable_index = 14;
  const int8_t return_code = 52;

  beast::Program prg;
  prg.declareVariable(return_code_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(return_code_variable_index, return_code, true);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.terminateWithVariableReturnCode(return_code_variable_index, true);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
  REQUIRE(session.getReturnCode() == return_code);
}

TEST_CASE("variables_can_be_variably_bit_shifted_left", "programs") {
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

TEST_CASE("variables_can_be_variably_bit_shifted_right", "programs") {
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

TEST_CASE("checking_if_stack_is_empty_works", "programs") {
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

TEST_CASE("variable_can_be_compared_for_gt_against_constant", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("variable_can_be_compared_for_lt_against_constant", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x0);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x1);
}

TEST_CASE("variable_can_be_compared_for_eq_against_constant", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x0);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x1);
}

TEST_CASE("variable_can_be_compared_for_gt_against_variable", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("variable_can_be_compared_for_lt_against_variable", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x0);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x1);
}

TEST_CASE("variable_can_be_compared_for_eq_against_variable", "programs") {
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
  while (vm.step(session)) {}

  REQUIRE(session.getVariableValue(target_variable_index_1, true) == 0x1);
  REQUIRE(session.getVariableValue(target_variable_index_2, true) == 0x0);
}

TEST_CASE("stacks_can_push_and_pop_constant_values", "programs") {
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

TEST_CASE("stacks_can_push_and_pop_variable_values", "programs") {
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
