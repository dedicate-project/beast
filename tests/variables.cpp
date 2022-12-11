#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("declare_variable", "instructions") {
  const int32_t variable_index = 43;

  beast::Program prg(6);
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  REQUIRE(prg.getData1(0) == 0x01);
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == 0);
}

TEST_CASE("set_variable", "instructions") {
  const int32_t variable_index = 43;
  const int32_t variable_content = 77612;
  const bool follow_links = false;

  beast::Program prg(10);
  prg.setVariable(variable_index, variable_content, follow_links);

  REQUIRE(prg.getData1(0) == 0x02);
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == (follow_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == variable_content);
}

TEST_CASE("undeclare_variable", "instructions") {
  const int32_t variable_index = 43;

  beast::Program prg(6);
  prg.undeclareVariable(variable_index);

  REQUIRE(prg.getData1(0) == 0x03);
  REQUIRE(prg.getData4(1) == variable_index);
}
