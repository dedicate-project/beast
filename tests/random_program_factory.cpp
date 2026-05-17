#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("random_programs_have_the_right_size", "random_program_factory") {
  beast::RandomProgramFactory factory;

  const uint32_t random_program_size_1 = 60;
  const uint32_t random_program_size_2 = 22;
  const uint32_t random_program_size_3 = 513;

  beast::Program program_1 = factory.generate(random_program_size_1, 10, 10, 10);
  beast::Program program_2 = factory.generate(random_program_size_2, 10, 10, 10);
  beast::Program program_3 = factory.generate(random_program_size_3, 10, 10, 10);

  REQUIRE(program_1.getSize() == random_program_size_1);
  REQUIRE(program_2.getSize() == random_program_size_2);
  REQUIRE(program_3.getSize() == random_program_size_3);
}

TEST_CASE("factory_generates_many_large_programs_without_hanging", "random_program_factory") {
  beast::RandomProgramFactory factory;

  const uint32_t random_program_size = 2500;
  const uint32_t program_count = 1000;

  for (uint32_t idx = 0; idx < program_count; ++idx) {
    beast::Program program = factory.generate(random_program_size, 100, 100, 100);
    REQUIRE(program.getSize() == random_program_size);
  }
}

namespace {
// Helpers below trim trailing zero bytes that Program::getData() includes for the unused tail of
// the program buffer; otherwise the parser sees a string of NoOp (opcode 0) bytes and our
// frequency-based assertions become noisy.
std::vector<unsigned char> filledBytes(const beast::Program& program) {
  std::vector<unsigned char> bytes = program.getData();
  bytes.resize(program.getPointer());
  return bytes;
}
} // namespace

TEST_CASE("opcode_weights_zero_excludes_an_opcode", "random_program_factory") {
  beast::RandomProgramFactory factory;
  beast::OpcodeWeights weights;
  // Enable only CopyVariable; everything else gets weight 0 and must not appear.
  for (int32_t code = 0; code < static_cast<int32_t>(beast::OpCode::Size); ++code) {
    weights[static_cast<beast::OpCode>(code)] = 0.0;
  }
  weights[beast::OpCode::CopyVariable] = 1.0;

  auto program = factory.generate(/*size=*/256, /*memory_size=*/4, /*string_table_size=*/0,
                                  /*string_table_item_length=*/0, weights);
  auto parse = beast::ProgramParser::parse(filledBytes(program));
  REQUIRE(parse.clean);
  REQUIRE_FALSE(parse.spans.empty());
  for (const auto& span : parse.spans) {
    REQUIRE(span.opcode == beast::OpCode::CopyVariable);
  }
}

TEST_CASE("opcode_weights_bias_sampling_distribution", "random_program_factory") {
  // With CopyVariable weighted vastly above the rest, it should dominate the resulting
  // operator mix. We don't expect EXACT 100% dominance because the other opcodes still have
  // weight 1 by default, but >75% is a safe lower bound for the chosen ratio over a long run.
  beast::RandomProgramFactory factory;
  beast::OpcodeWeights weights;
  weights[beast::OpCode::CopyVariable] = 1000.0;

  uint32_t copy_count = 0;
  uint32_t total_count = 0;
  for (int trial = 0; trial < 8; ++trial) {
    auto program = factory.generate(/*size=*/512, /*memory_size=*/8, /*string_table_size=*/0,
                                    /*string_table_item_length=*/0, weights);
    auto parse = beast::ProgramParser::parse(filledBytes(program));
    REQUIRE(parse.clean);
    for (const auto& span : parse.spans) {
      if (span.opcode == beast::OpCode::CopyVariable) {
        copy_count++;
      }
      total_count++;
    }
  }
  REQUIRE(total_count > 0);
  REQUIRE(static_cast<double>(copy_count) / static_cast<double>(total_count) > 0.75);
}

TEST_CASE("opcode_weights_default_to_uniform_when_empty", "random_program_factory") {
  // Empty weights map must behave exactly like the uniform overload (i.e. not throw and produce
  // a program that fills the buffer).
  beast::RandomProgramFactory factory;
  beast::OpcodeWeights weights;

  auto program = factory.generate(/*size=*/128, /*memory_size=*/4, /*string_table_size=*/0,
                                  /*string_table_item_length=*/0, weights);
  REQUIRE(program.getSize() == 128);
  REQUIRE(program.getPointer() > 0);
}
