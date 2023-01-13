#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("retrieving_1_byte_too_many_from_program_throws", "program") {
  beast::Program prg(0);
  bool threw = false;
  try {
    prg.getData1(0);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("retrieving_2_bytes_too_many_from_program_throws", "program") {
  beast::Program prg(0);
  bool threw = false;
  try {
    prg.getData2(0);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("retrieving_4_bytes_too_many_from_program_throws", "program") {
  beast::Program prg(0);
  bool threw = false;
  try {
    prg.getData4(0);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("adding_too_large_string_table_entry_instruction_throws", "program") {
  beast::Program prg(0);
  const std::string entry = "Entry";
  bool threw = false;
  try {
    prg.setStringTableEntry(0, entry);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("dymamically_growing_programs_assume_a_fitting_size", "program") {
  beast::Program prg;
  prg.declareVariable(10, beast::Program::VariableType::Int32);
  prg.setVariable(10, 0x48, true);

  REQUIRE(prg.getSize() == 16);
}

TEST_CASE("inserting_a_too_large_program_throws", "program") {
  beast::Program prg1;
  prg1.declareVariable(0, beast::Program::VariableType::Int32);
  prg1.setVariable(0, 0, true);

  beast::Program prg2(10);
  bool threw = false;
  try {
    prg2.insertProgram(prg1);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("inserted_programs_work_as_intended", "program") {
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

TEST_CASE("programs_initialized_with_byte_code_have_the_right_size", "program") {
  std::vector<unsigned char> bytecode = {0x0, 0x1, 0x2, 0x3, 0x4};
  const uint32_t size = bytecode.size();
  beast::Program prg(std::move(bytecode));

  REQUIRE(prg.getSize() == size);
}

TEST_CASE("programs_initialized_with_byte_code_have_the_right_content", "program") {
  std::vector<unsigned char> bytecode = {0x0, 0x1, 0x2, 0x3, 0x4};
  const uint32_t size = bytecode.size();
  beast::Program prg(bytecode);

  for (size_t idx = 0; idx < size; ++idx) {
    REQUIRE(prg.getData()[idx] == bytecode[idx]);
  }
}
