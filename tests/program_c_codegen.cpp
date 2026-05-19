// Catch2
#include <catch2/catch.hpp>

// Standard
#include <string>
#include <vector>

// BEAST
#include <beast/program.hpp>
#include <beast/program_c_codegen.hpp>

TEST_CASE("ProgramCCodeGenerator emits a complete C source for an empty program") {
  // Smoke test: even an empty bytecode should produce a complete C file. The empty
  // case is special because the bytecode array can't be `{}` in standard C -- the
  // generator inserts a `0x00` placeholder so the compiler accepts the file.
  beast::ProgramCCodeGenerator::Options opts;
  const auto src = beast::ProgramCCodeGenerator::generate({}, opts);
  CHECK(src.find("int main(") != std::string::npos);
  CHECK(src.find("static const unsigned char PROGRAM[] = {") != std::string::npos);
  // The "0x00" placeholder appears once in the bytecode array (which is the only place
  // the generator emits a bare literal -- everything else is part of the template).
  CHECK(src.find("  0x00") != std::string::npos);
  // Defaults must take effect: 64 vars, 256 stack, 1M steps. None of these are
  // user-visible CLI flags so the only correctness signal is what got baked in.
  CHECK(src.find("#define MEM_SIZE   64") != std::string::npos);
  CHECK(src.find("#define STACK_SIZE 256") != std::string::npos);
  CHECK(src.find("#define MAX_STEPS  1000000") != std::string::npos);
}

TEST_CASE("ProgramCCodeGenerator embeds the bytecode with one byte per comma-separated value") {
  // Dynamic-growth program so we don't pad with NoOps; the codegen only cares about
  // the actual bytes the user wants emitted.
  beast::Program prog;
  prog.setVariable(/*variable_index=*/0, /*content=*/0x2a, /*follow_links=*/true);
  prog.terminate(/*return_code=*/0);
  const auto bytecode = prog.getData();
  REQUIRE_FALSE(bytecode.empty());

  beast::ProgramCCodeGenerator::Options opts;
  opts.memory_variables = 32;
  opts.stack_capacity = 64;
  opts.max_steps = 5000;
  opts.source_description = "from test (score 0.5)\n2 instructions";
  const auto src = beast::ProgramCCodeGenerator::generate(bytecode, opts);

  // Configuration baked in
  CHECK(src.find("#define MEM_SIZE   32") != std::string::npos);
  CHECK(src.find("#define STACK_SIZE 64") != std::string::npos);
  CHECK(src.find("#define MAX_STEPS  5000") != std::string::npos);

  // Banner threaded into the top comment, one " * " line per source line
  CHECK(src.find("from test (score 0.5)") != std::string::npos);
  CHECK(src.find("2 instructions") != std::string::npos);

  // The first byte of the genome must appear somewhere in the bytecode array. We
  // don't pin the exact format (the byte count per line could change) but we do pin
  // that the literal "0x08" (SetVariable opcode) is present at least once.
  CHECK(src.find("0x08") != std::string::npos);
  // The Terminate opcode (0x03) is also present.
  CHECK(src.find("0x03") != std::string::npos);
}

TEST_CASE("ProgramCCodeGenerator template is substitution-complete") {
  // Belt-and-braces guard: no `{{...}}` placeholder should survive into the emitted
  // source. If we add a new placeholder to the template later and forget to wire it
  // in the C++ side, this test catches it instantly.
  const auto src = beast::ProgramCCodeGenerator::generate({0x00, 0x03, 0x00}, {});
  CHECK(src.find("{{") == std::string::npos);
  CHECK(src.find("}}") == std::string::npos);
}
