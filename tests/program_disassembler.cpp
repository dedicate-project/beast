// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/program.hpp>
#include <beast/program_disassembler.hpp>

TEST_CASE("ProgramDisassembler decodes SetVariable + AddVariableToVariable") {
  // Hand-build a tiny program via the Program builder to be sure the encoding matches
  // what the VM consumes; testing on synthesised bytes runs the risk of being a wrong
  // mirror of the actual encoding. Use the default ctor so the program grows
  // dynamically -- pre-allocating a fixed space would zero-pad the tail and the
  // disassembler would happily decode the padding as a sea of NoOps.
  beast::Program prog;
  prog.setVariable(/*variable_index=*/0, /*content=*/42, /*follow_links=*/true);
  prog.addVariableToVariable(/*source_variable_index=*/8, /*follow_source_links=*/true,
                              /*destination_variable_index=*/0,
                              /*follow_destination_links=*/true);
  prog.terminate(/*return_code=*/0);

  const auto result = beast::ProgramDisassembler::disassemble(prog.getData());
  REQUIRE(result.clean);
  REQUIRE(result.trailing_garbage_bytes == 0);
  REQUIRE(result.instructions.size() == 3);

  // First instruction
  const auto& first = result.instructions.at(0);
  CHECK(first.opcode == beast::OpCode::SetVariable);
  CHECK(first.mnemonic == "SetVariable");
  CHECK(first.offset == 0);
  REQUIRE(first.operands.size() == 3);
  CHECK(first.operands.at(0) == 0);   // variable index
  CHECK(first.operands.at(1) == 1);   // follow flag (true)
  CHECK(first.operands.at(2) == 42);  // value
  CHECK(first.text == "SetVariable v=0 follow=1 value=42");
  // bytes_hex spans 1 (opcode) + 4 (var) + 1 (follow) + 4 (value) = 10 bytes = 20 hex chars
  CHECK(first.bytes_hex.size() == 20);

  // Second instruction (an "add variable to variable", offset = 10)
  const auto& second = result.instructions.at(1);
  CHECK(second.opcode == beast::OpCode::AddVariableToVariable);
  CHECK(second.offset == 10);
  CHECK(second.text == "AddVariableToVariable a=8 follow_a=1 b=0 follow_b=1");

  // Third: terminate
  const auto& third = result.instructions.at(2);
  CHECK(third.opcode == beast::OpCode::Terminate);
  CHECK(third.text == "Terminate return_code=0");
}

TEST_CASE("ProgramDisassembler reports trailing garbage and stays clean=false") {
  // A NoOp byte followed by an unknown opcode. The parser should record one valid
  // instruction and flag the rest as garbage; the disassembler should pass that
  // through unchanged.
  std::vector<unsigned char> bytes;
  bytes.push_back(0x00); // NoOp
  bytes.push_back(0x7F); // unrecognised opcode -- not in the OpCode enum
  bytes.push_back(0xAA);
  bytes.push_back(0xBB);

  const auto result = beast::ProgramDisassembler::disassemble(bytes);
  REQUIRE_FALSE(result.clean);
  REQUIRE(result.instructions.size() == 1);
  CHECK(result.instructions.front().opcode == beast::OpCode::NoOp);
  CHECK(result.trailing_garbage_bytes == 3);
}

TEST_CASE("ProgramDisassembler is total on empty input") {
  // An empty genome must still produce a sane (empty) result rather than a UB
  // out-of-range read. The Program Collection UI relies on this for newly-created,
  // never-populated ledgers.
  const auto result = beast::ProgramDisassembler::disassemble({});
  CHECK(result.clean);
  CHECK(result.instructions.empty());
  CHECK(result.trailing_garbage_bytes == 0);
}
