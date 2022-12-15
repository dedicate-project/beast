#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/program.hpp>

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
