#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("declare_variable", "instructions") {
  using namespace beast;

  const int variable_index = 43;

  Program prg(6);
  prg.declareVariable(variable_index, Program::VariableType::Int32);

  REQUIRE(prg.getData1(0) == 0x01);
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == 0);
}

TEST_CASE("undeclare_variable", "instructions") {
  using namespace beast;

  const int variable_index = 43;

  Program prg(6);
  prg.undeclareVariable(variable_index);

  REQUIRE(prg.getData1(0) == 0x03);
  REQUIRE(prg.getData4(1) == variable_index);
}
