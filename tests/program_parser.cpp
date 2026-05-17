#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("parser_empty_input_yields_clean_empty_result", "program_parser") {
  const std::vector<unsigned char> bytes;
  const auto result = beast::ProgramParser::parse(bytes);

  REQUIRE(result.clean);
  REQUIRE(result.spans.empty());
  REQUIRE(result.trailing_garbage_bytes == 0);
}

TEST_CASE("parser_decodes_a_well_formed_program", "program_parser") {
  beast::Program program;
  program.declareVariable(0, beast::Program::VariableType::Int32);
  program.setVariable(0, 42, false);
  program.addConstantToVariable(0, 5, false);
  program.terminate(0);

  const auto result = beast::ProgramParser::parse(program.getData());
  REQUIRE(result.clean);
  REQUIRE(result.spans.size() == 4);
  REQUIRE(result.spans[0].opcode == beast::OpCode::DeclareVariable);
  REQUIRE(result.spans[1].opcode == beast::OpCode::SetVariable);
  REQUIRE(result.spans[2].opcode == beast::OpCode::AddConstantToVariable);
  REQUIRE(result.spans[3].opcode == beast::OpCode::Terminate);
  REQUIRE(result.trailing_garbage_bytes == 0);
}

TEST_CASE("parser_reports_trailing_garbage_on_truncated_operand", "program_parser") {
  beast::Program program;
  program.declareVariable(0, beast::Program::VariableType::Int32);
  // Append the AddConstantToVariable opcode byte but truncate the rest of the operand.
  std::vector<unsigned char> bytes = program.getData();
  bytes.push_back(static_cast<unsigned char>(beast::OpCode::AddConstantToVariable));
  bytes.push_back(0x01); // Partial operand, intentionally too short.

  const auto result = beast::ProgramParser::parse(bytes);
  REQUIRE_FALSE(result.clean);
  REQUIRE(result.spans.size() == 1);
  REQUIRE(result.spans[0].opcode == beast::OpCode::DeclareVariable);
  REQUIRE(result.trailing_garbage_bytes == 2);
}

TEST_CASE("parser_handles_variable_length_string_entries", "program_parser") {
  beast::Program program;
  program.declareVariable(0, beast::Program::VariableType::Int32);
  program.setStringTableEntry(0, "hello");
  program.printStringFromStringTable(0);

  const auto result = beast::ProgramParser::parse(program.getData());
  REQUIRE(result.clean);
  REQUIRE(result.spans.size() == 3);
  REQUIRE(result.spans[1].opcode == beast::OpCode::SetStringTableEntry);
  // 1 byte opcode + 4 bytes index + 2 bytes length + 5 bytes "hello" = 12 bytes total.
  REQUIRE(result.spans[1].length == 12);
}

TEST_CASE("parser_reports_unknown_opcode_as_trailing_garbage", "program_parser") {
  // 0xFF maps to no known OpCode in the current enum and should be treated as trailing garbage.
  const std::vector<unsigned char> bytes{static_cast<unsigned char>(beast::OpCode::NoOp), 0xFF};
  const auto result = beast::ProgramParser::parse(bytes);
  REQUIRE_FALSE(result.clean);
  REQUIRE(result.spans.size() == 1);
  REQUIRE(result.trailing_garbage_bytes == 1);
}
