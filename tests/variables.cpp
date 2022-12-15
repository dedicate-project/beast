#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>
#include <beast/opcodes.hpp>

TEST_CASE("declare_variable", "instructions") {
  const int32_t variable_index = 43;

  beast::Program prg(6);
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::DeclareVariable));
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == 0);
}

TEST_CASE("set_variable", "instructions") {
  const int32_t variable_index = 43;
  const int32_t variable_content = 77612;
  const bool follow_links = false;

  beast::Program prg(10);
  prg.setVariable(variable_index, variable_content, follow_links);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::SetVariable));
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == (follow_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == variable_content);
}

TEST_CASE("undeclare_variable", "instructions") {
  const int32_t variable_index = 43;

  beast::Program prg(6);
  prg.undeclareVariable(variable_index);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::UndeclareVariable));
  REQUIRE(prg.getData4(1) == variable_index);
}

TEST_CASE("add_constant_to_variable", "instructions") {
  const int32_t variable_index = 22;
  const int32_t constant = -91;
  const bool follow_links = true;

  beast::Program prg(10);
  prg.addConstantToVariable(variable_index, constant, follow_links);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::AddConstantToVariable));
  REQUIRE(prg.getData4(1) == variable_index);
  REQUIRE(prg.getData1(5) == (follow_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == constant);
}

TEST_CASE("add_variable_to_variable", "instructions") {
  const int32_t source_variable_index = 10;
  const bool follow_source_links = true;
  const int32_t destination_variable_index = 20;
  const bool follow_destination_links = true;

  beast::Program prg(11);
  prg.addVariableToVariable(source_variable_index, follow_source_links, destination_variable_index, follow_destination_links);

  REQUIRE(prg.getData1(0) == static_cast<int8_t>(beast::OpCode::AddVariableToVariable));
  REQUIRE(prg.getData4(1) == source_variable_index);
  REQUIRE(prg.getData1(5) == (follow_source_links ? 0x1 : 0x0));
  REQUIRE(prg.getData4(6) == destination_variable_index);
  REQUIRE(prg.getData1(10) == (follow_destination_links ? 0x1 : 0x0));
}
