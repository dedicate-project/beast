#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_FAST_COMPILE

#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("variable_string_table_item_length_can_be_determined", "printing_and_string_table") {
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

TEST_CASE("variable_string_table_item_can_be_loaded_into_variables", "printing_and_string_table") {
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

TEST_CASE("print_variable_index_from_string_table", "printing_and_string_table") {
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

TEST_CASE("string_table_item_can_be_set_at_variable_index", "printing_and_string_table") {
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

TEST_CASE("string_table_item_length_can_be_determined", "printing_and_string_table") {
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

TEST_CASE("getting_invalid_string_table_item_length_throws", "printing_and_string_table") {
  beast::Program prg;
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.loadStringItemLengthIntoVariable(1, 0, true);

  beast::VmSession session(std::move(prg), 1, 1, 1);
  beast::CpuVirtualMachine vm;
  vm.step(session);
  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("string_table_item_can_be_loaded_into_variables", "printing_and_string_table") {
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

TEST_CASE("string_table_limit_can_be_determined", "printing_and_string_table") {
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

TEST_CASE("string_table_item_length_limit_can_be_determined", "printing_and_string_table") {
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

TEST_CASE("print_conditionally_if_var_gt_0_with_fixed_rel_addr", "printing_and_string_table") {
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

TEST_CASE("print_conditionally_if_var_lt_0_with_fixed_rel_addr", "printing_and_string_table") {
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

TEST_CASE("print_conditionally_if_var_eq_0_with_fixed_rel_addr", "printing_and_string_table") {
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

TEST_CASE("print_from_string_table", "printing_and_string_table") {
  const std::string output = "Output";

  beast::Program prg(100);
  prg.setStringTableEntry(0, output);
  prg.printStringFromStringTable(0);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  while (vm.step(session)) {}

  REQUIRE(session.getPrintBuffer() == output);
}

TEST_CASE("print_chars_from_variables", "printing_and_string_table") {
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

TEST_CASE("print_conditionally_if_var_not_gt_0", "printing_and_string_table") {
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

TEST_CASE("print_conditionally_if_var_not_lt_0", "printing_and_string_table") {
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

TEST_CASE("print_conditionally_if_var_not_eq_0", "printing_and_string_table") {
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

TEST_CASE("set_string_table_entry_outside_bounds_throws", "printing_and_string_table") {
  const int32_t string_table_index = 22;
  const std::string output = "Output";

  beast::Program prg;
  prg.setStringTableEntry(string_table_index, output);

  beast::VmSession session(std::move(prg), 0, 21, 10);
  beast::CpuVirtualMachine vm;
  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("set_variable_string_table_entry_outside_bounds_throws", "printing_and_string_table") {
  const int32_t variable_index = 0;
  const int32_t string_table_index = 22;
  const std::string output = "Output";

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index, string_table_index, true);
  prg.setVariableStringTableEntry(variable_index, true, output);

  beast::VmSession session(std::move(prg), 1, 21, 10);
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
