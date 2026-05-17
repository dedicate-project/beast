#include <catch2/catch.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include <nlohmann/json.hpp>

#include <beast/beast.hpp>

namespace {

/// Build a tiny "increment-the-input-and-emit" subroutine bytecode for tests.
///
/// Layout per `BitDistanceEvaluator` (which the dispatch mirrors): inputs at
/// vars 0..in_arity-1, trial-id at in_arity, outputs at in_arity+1...
///
/// Body:
///   - copy input0 -> output0
///   - add `increment_by` to output0
///   - terminate
std::vector<unsigned char> makeIncrementBytecode(int32_t increment_by) {
  beast::Program program;
  // Copy input var 0 -> output var 2, then add `increment_by` to var 2.
  program.copyVariable(0, true, 2, true);
  program.addConstantToVariable(2, increment_by, true);
  program.terminate(0);
  return program.getData();
}

/// Build a `(in_a, in_b) -> a + b` subroutine bytecode (arity 2 in, 1 out).
std::vector<unsigned char> makeAdderBytecode() {
  beast::Program program;
  // Layout: vars 0,1 are inputs, var 2 is the trial-id slot, var 3 is the single output.
  program.copyVariable(0, true, 3, true);
  program.addVariableToVariable(1, true, 3, true);
  program.terminate(0);
  return program.getData();
}

/// Build a never-writes-its-output subroutine to exercise the "callee timed out
/// with partial outputs" path.
std::vector<unsigned char> makeNoopBytecode() {
  beast::Program program;
  program.noop();
  program.terminate(0);
  return program.getData();
}

/// Tiny harness: build a caller `Program` that sets up an input variable, calls
/// `CallSubroutine` with the supplied id/arity, and returns the resulting output.
struct CallerResult {
  int32_t output_value;
  bool exited_abnormally;
  uint32_t steps;
};

CallerResult runCaller(const beast::SubroutineLibrary& library, uint8_t subroutine_id,
                       uint8_t input_arity, uint8_t output_arity,
                       const std::vector<int32_t>& input_values,
                       int32_t output_var_index = 50, size_t variable_count = 64) {
  beast::Program caller;
  // Provision input variables and seed them.
  for (uint8_t i = 0; i < input_arity; ++i) {
    const int32_t var_index = static_cast<int32_t>(10 + i);
    caller.declareVariable(var_index, beast::Program::VariableType::Int32);
    caller.setVariable(var_index, input_values.at(i), false);
  }
  caller.declareVariable(output_var_index, beast::Program::VariableType::Int32);

  std::vector<beast::Program::SubroutineArgument> inputs;
  inputs.reserve(input_arity);
  for (uint8_t i = 0; i < input_arity; ++i) {
    inputs.push_back({static_cast<int32_t>(10 + i), false});
  }
  std::vector<beast::Program::SubroutineArgument> outputs;
  outputs.reserve(output_arity);
  for (uint8_t i = 0; i < output_arity; ++i) {
    outputs.push_back({output_var_index + static_cast<int32_t>(i), false});
  }
  caller.callSubroutine(subroutine_id, inputs, outputs);
  caller.terminate(0);

  beast::VmSession session(beast::Program(caller.getData()), variable_count,
                           /*string_table_count=*/4, /*max_string_size=*/16);
  session.setSubroutineLibrary(std::make_shared<const beast::SubroutineLibrary>(library));

  beast::CpuVirtualMachine machine;
  machine.setSilent(true);
  uint32_t steps = 0;
  while (machine.step(session, false)) {
    ++steps;
    if (steps > 10000) {
      break;
    }
  }

  CallerResult result{};
  result.steps = steps;
  result.exited_abnormally = session.getRuntimeStatistics().abnormal_exit;
  result.output_value = session.getVariableValue(output_var_index, false);
  return result;
}

} // namespace

TEST_CASE("call_subroutine_encode_decode_round_trip", "call_subroutine") {
  beast::Program program;
  std::vector<beast::Program::SubroutineArgument> inputs{{1, false}, {2, true}};
  std::vector<beast::Program::SubroutineArgument> outputs{{5, true}};
  program.callSubroutine(0, inputs, outputs);

  const auto result = beast::ProgramParser::parse(program.getData());
  REQUIRE(result.clean);
  REQUIRE(result.spans.size() == 1);
  REQUIRE(result.spans[0].opcode == beast::OpCode::CallSubroutine);
  // Header (4) + 2*5 inputs + 1*5 outputs = 19 bytes total.
  REQUIRE(result.spans[0].length == 4 + 5 * 2 + 5 * 1);
}

TEST_CASE("call_subroutine_arity_overflow_throws_at_encode_time", "call_subroutine") {
  beast::Program program;
  std::vector<beast::Program::SubroutineArgument> big(256, {0, false});
  std::vector<beast::Program::SubroutineArgument> outputs{{5, true}};
  REQUIRE_THROWS_AS(program.callSubroutine(0, big, outputs), std::length_error);
}

TEST_CASE("call_subroutine_zero_arity_is_legal", "call_subroutine") {
  beast::Program program;
  program.callSubroutine(7, {}, {});
  const auto result = beast::ProgramParser::parse(program.getData());
  REQUIRE(result.clean);
  REQUIRE(result.spans.size() == 1);
  // Header bytes only.
  REQUIRE(result.spans[0].length == 4);
}

TEST_CASE("call_subroutine_parser_handles_mixed_program", "call_subroutine") {
  beast::Program program;
  program.declareVariable(0, beast::Program::VariableType::Int32);
  program.setVariable(0, 42, false);
  std::vector<beast::Program::SubroutineArgument> inputs{{0, false}, {0, true}, {0, false}};
  std::vector<beast::Program::SubroutineArgument> outputs{{0, false}, {0, true}};
  program.callSubroutine(3, inputs, outputs);
  program.terminate(0);

  const auto result = beast::ProgramParser::parse(program.getData());
  REQUIRE(result.clean);
  REQUIRE(result.spans.size() == 4);
  REQUIRE(result.spans[2].opcode == beast::OpCode::CallSubroutine);
  // 4 header + 3*5 in + 2*5 out = 29 bytes.
  REQUIRE(result.spans[2].length == 4 + 5 * 3 + 5 * 2);
}

TEST_CASE("call_subroutine_parser_rejects_truncated_payload", "call_subroutine") {
  beast::Program program;
  std::vector<beast::Program::SubroutineArgument> inputs{{0, false}, {1, false}};
  std::vector<beast::Program::SubroutineArgument> outputs{{5, true}};
  program.callSubroutine(0, inputs, outputs);
  std::vector<unsigned char> bytes = program.getData();
  // Drop the last 3 bytes mid-payload.
  bytes.resize(bytes.size() - 3);

  const auto result = beast::ProgramParser::parse(bytes);
  REQUIRE_FALSE(result.clean);
  REQUIRE(result.spans.empty());
  REQUIRE(result.trailing_garbage_bytes == bytes.size());
}

TEST_CASE("call_subroutine_with_no_library_marks_session_abnormal", "call_subroutine") {
  beast::Program caller;
  caller.callSubroutine(0, {}, {});
  caller.terminate(0);

  beast::VmSession session(beast::Program(caller.getData()), 16, 4, 16);
  // Deliberately do NOT mount a library.
  beast::CpuVirtualMachine machine;
  machine.setSilent(true);
  while (machine.step(session, false)) {
    // Walk until step returns false.
  }
  REQUIRE(session.getRuntimeStatistics().abnormal_exit);
}

TEST_CASE("call_subroutine_id_out_of_range_marks_session_abnormal", "call_subroutine") {
  beast::SubroutineLibrary library;
  library.push_back({makeIncrementBytecode(1), /*input_arity=*/1, /*output_arity=*/1,
                     /*max_steps_per_call=*/100});

  const auto result = runCaller(library, /*subroutine_id=*/3, /*input_arity=*/1,
                                /*output_arity=*/1, {5});
  REQUIRE(result.exited_abnormally);
}

TEST_CASE("call_subroutine_arity_mismatch_marks_session_abnormal", "call_subroutine") {
  beast::SubroutineLibrary library;
  library.push_back({makeAdderBytecode(), /*input_arity=*/2, /*output_arity=*/1,
                     /*max_steps_per_call=*/100});

  // Caller says arity (1, 1) but library says (2, 1).
  const auto result = runCaller(library, /*subroutine_id=*/0, /*input_arity=*/1,
                                /*output_arity=*/1, {5});
  REQUIRE(result.exited_abnormally);
}

TEST_CASE("call_subroutine_executes_callee_and_copies_outputs", "call_subroutine") {
  beast::SubroutineLibrary library;
  library.push_back({makeIncrementBytecode(7), /*input_arity=*/1, /*output_arity=*/1,
                     /*max_steps_per_call=*/200});

  const auto result = runCaller(library, /*subroutine_id=*/0, /*input_arity=*/1,
                                /*output_arity=*/1, {35});
  REQUIRE_FALSE(result.exited_abnormally);
  REQUIRE(result.output_value == 42); // 35 + 7
}

TEST_CASE("call_subroutine_multi_arity_adds", "call_subroutine") {
  beast::SubroutineLibrary library;
  library.push_back({makeAdderBytecode(), /*input_arity=*/2, /*output_arity=*/1,
                     /*max_steps_per_call=*/200});

  const auto result = runCaller(library, /*subroutine_id=*/0, /*input_arity=*/2,
                                /*output_arity=*/1, {17, 25});
  REQUIRE_FALSE(result.exited_abnormally);
  REQUIRE(result.output_value == 42); // 17 + 25
}

TEST_CASE("call_subroutine_with_unwritten_output_reads_default_zero", "call_subroutine") {
  // The noop subroutine has output_arity=1 but never writes to its output. The
  // caller should observe the default value (0) and continue without abnormal exit.
  beast::SubroutineLibrary library;
  library.push_back({makeNoopBytecode(), /*input_arity=*/0, /*output_arity=*/1,
                     /*max_steps_per_call=*/50});

  const auto result = runCaller(library, /*subroutine_id=*/0, /*input_arity=*/0,
                                /*output_arity=*/1, {});
  REQUIRE_FALSE(result.exited_abnormally);
  REQUIRE(result.output_value == 0);
}

TEST_CASE("subroutine_body_is_call_free_detects_recursion", "call_subroutine") {
  // Empty body is vacuously safe.
  REQUIRE(beast::subroutineBodyIsCallFree({}));

  beast::Program plain;
  plain.copyVariable(0, false, 1, false);
  plain.terminate(0);
  REQUIRE(beast::subroutineBodyIsCallFree(plain.getData()));

  beast::Program recursive;
  recursive.callSubroutine(0, {}, {});
  recursive.terminate(0);
  REQUIRE_FALSE(beast::subroutineBodyIsCallFree(recursive.getData()));
}

TEST_CASE("factory_emits_valid_call_subroutine_when_library_mounted", "call_subroutine") {
  beast::RandomProgramFactory factory;
  // Only enable CallSubroutine; every other opcode gets weight 0.
  beast::OpcodeWeights weights;
  for (int32_t code = 0; code < static_cast<int32_t>(beast::OpCode::Size); ++code) {
    weights[static_cast<beast::OpCode>(code)] = 0.0;
  }
  weights[beast::OpCode::CallSubroutine] = 1.0;

  // Two-entry arity table: id 0 = (1, 1), id 1 = (3, 2).
  beast::SubroutineArityTable arities{{1, 1}, {3, 2}};

  const beast::Program program = factory.generate(/*size=*/4096, /*memory_size=*/64,
                                                  /*string_table_size=*/4,
                                                  /*string_table_item_length=*/16, weights,
                                                  arities);
  std::vector<unsigned char> bytes = program.getData();
  bytes.resize(program.getPointer());

  const auto parsed = beast::ProgramParser::parse(bytes);
  REQUIRE(parsed.clean);
  REQUIRE_FALSE(parsed.spans.empty());
  // Every emitted span must be CallSubroutine, and the subroutine_id byte (offset 1)
  // must be within `arities.size()`.
  uint32_t calls = 0;
  for (const auto& span : parsed.spans) {
    REQUIRE(span.opcode == beast::OpCode::CallSubroutine);
    const uint8_t subroutine_id = bytes.at(span.offset + 1);
    REQUIRE(subroutine_id < arities.size());
    const uint8_t input_arity = bytes.at(span.offset + 2);
    const uint8_t output_arity = bytes.at(span.offset + 3);
    REQUIRE(input_arity == arities.at(subroutine_id).first);
    REQUIRE(output_arity == arities.at(subroutine_id).second);
    ++calls;
  }
  REQUIRE(calls >= 1);
}

TEST_CASE("factory_drops_call_subroutine_when_library_empty", "call_subroutine") {
  beast::RandomProgramFactory factory;
  beast::OpcodeWeights weights;
  for (int32_t code = 0; code < static_cast<int32_t>(beast::OpCode::Size); ++code) {
    weights[static_cast<beast::OpCode>(code)] = 0.0;
  }
  // Try to force CallSubroutine -- but with no library, the factory must zero it out
  // and (since *every* opcode is weighted 0) emit nothing at all.
  weights[beast::OpCode::CallSubroutine] = 1.0;

  const beast::Program program =
      factory.generate(/*size=*/256, /*memory_size=*/16, /*string_table_size=*/4,
                       /*string_table_item_length=*/16, weights, beast::SubroutineArityTable{});
  // With every opcode (including CallSubroutine, now zero-weighted) at zero, the discrete
  // distribution falls back to uniform-over-all-zero which std::discrete_distribution treats
  // as "pick index 0 (NoOp)". The body should therefore be all NoOps -- definitively no
  // CallSubroutine.
  std::vector<unsigned char> bytes = program.getData();
  bytes.resize(program.getPointer());
  const auto parsed = beast::ProgramParser::parse(bytes);
  for (const auto& span : parsed.spans) {
    REQUIRE(span.opcode != beast::OpCode::CallSubroutine);
  }
}

TEST_CASE("generate_random_operator_emits_valid_call_subroutine", "call_subroutine") {
  beast::OpcodeWeights weights;
  for (int32_t code = 0; code < static_cast<int32_t>(beast::OpCode::Size); ++code) {
    weights[static_cast<beast::OpCode>(code)] = 0.0;
  }
  weights[beast::OpCode::CallSubroutine] = 1.0;

  // Single-entry library: id 0 = (8, 8) -- the worst-case arity. Total bytes:
  // 4 header + 16*5 = 84 bytes. Need max_bytes >= 84 to fit.
  beast::SubroutineArityTable arities{{8, 8}};
  for (int trial = 0; trial < 32; ++trial) {
    const auto bytes = beast::RandomProgramFactory::generateRandomOperator(
        /*memory_size=*/64, /*string_table_size=*/4, /*string_table_item_length=*/16,
        /*max_bytes=*/128, weights, arities);
    REQUIRE_FALSE(bytes.empty());
    const auto parsed = beast::ProgramParser::parse(bytes);
    REQUIRE(parsed.clean);
    REQUIRE(parsed.spans.size() == 1);
    REQUIRE(parsed.spans[0].opcode == beast::OpCode::CallSubroutine);
    REQUIRE(parsed.spans[0].length == 4 + 8 * 5 + 8 * 5);
  }
}

namespace {
/// Write a ledger file in `ProgramStorageSinkPipe` format with a single entry.
///
/// Returns the resolved path so the test can clean it up after the round-trip.
std::filesystem::path writeLedgerWithEntry(const std::string& filename,
                                           const std::vector<unsigned char>& bytecode,
                                           double score) {
  auto path = std::filesystem::temp_directory_path() / filename;
  nlohmann::json doc = nlohmann::json::array();
  doc.push_back({{"score", score}, {"data", bytecode}});
  std::ofstream out(path);
  out << doc.dump(2);
  return path;
}
} // namespace

TEST_CASE("evaluator_pipe_mounts_subroutine_from_ledger", "call_subroutine") {
  // Stage 1 survivor: copies input -> output (identity). Stage 2 mounts it as a
  // subroutine and exercises it via a synthesised caller bytecode.
  beast::Program identity;
  identity.copyVariable(0, true, 2, true);
  identity.terminate(0);
  const auto ledger = writeLedgerWithEntry(
      "beast_call_subroutine_ledger.json", identity.getData(), /*score=*/0.95);

  beast::EvaluatorPipe pipe(/*max_candidates=*/4, /*variable_count=*/16,
                            /*string_table_count=*/4, /*max_string_size=*/16);
  pipe.addSubroutineSource(
      {ledger.string(), /*top_k=*/1, /*input_arity=*/1, /*output_arity=*/1,
       /*max_steps_per_call=*/100});

  // Build the library directly (would normally happen at execute() time).
  pipe.rebuildSubroutineLibrary();
  auto library = pipe.getSubroutineLibrary();
  REQUIRE(library != nullptr);
  REQUIRE(library->size() == 1);
  REQUIRE(library->front().input_arity == 1);
  REQUIRE(library->front().output_arity == 1);

  // The EvolutionParameters arity table must mirror the library so the GA can
  // emit valid CallSubroutine instructions.
  const auto& params = pipe.getEvolutionParameters();
  REQUIRE(params.subroutine_arities.size() == 1);
  REQUIRE(params.subroutine_arities.front() == std::pair<uint8_t, uint8_t>{1, 1});

  std::filesystem::remove(ledger);
}

TEST_CASE("evaluator_pipe_rejects_recursive_subroutine_at_load", "call_subroutine") {
  // A ledger entry that itself contains CallSubroutine: must be filtered out so
  // recursion is impossible.
  beast::Program recursive;
  recursive.callSubroutine(0, {}, {});
  recursive.terminate(0);
  const auto ledger = writeLedgerWithEntry(
      "beast_call_subroutine_recursive.json", recursive.getData(), /*score=*/0.99);

  beast::EvaluatorPipe pipe(/*max_candidates=*/4, /*variable_count=*/16,
                            /*string_table_count=*/4, /*max_string_size=*/16);
  pipe.addSubroutineSource(
      {ledger.string(), /*top_k=*/1, /*input_arity=*/0, /*output_arity=*/1,
       /*max_steps_per_call=*/100});
  pipe.rebuildSubroutineLibrary();
  REQUIRE(pipe.getSubroutineLibrary()->empty());

  std::filesystem::remove(ledger);
}

TEST_CASE("evaluator_pipe_missing_ledger_yields_empty_library", "call_subroutine") {
  beast::EvaluatorPipe pipe(/*max_candidates=*/4, /*variable_count=*/16,
                            /*string_table_count=*/4, /*max_string_size=*/16);
  pipe.addSubroutineSource({"/definitely/does/not/exist.json", /*top_k=*/3,
                            /*input_arity=*/1, /*output_arity=*/1,
                            /*max_steps_per_call=*/100});
  pipe.rebuildSubroutineLibrary();
  REQUIRE(pipe.getSubroutineLibrary()->empty());
  REQUIRE(pipe.getEvolutionParameters().subroutine_arities.empty());
}

TEST_CASE("call_subroutine_callee_does_not_see_caller_library", "call_subroutine") {
  // Callee bytecode tries to call subroutine 0 itself. With v1 "no recursion",
  // the dispatch path explicitly does NOT propagate the library to the callee,
  // so the callee's CallSubroutine must fail closed -- the caller observes a
  // 0 output (unwritten) but no abnormal exit on the *outer* path. The callee's
  // own abnormal exit is contained to the callee session.
  beast::Program recursive_body;
  recursive_body.callSubroutine(0, {}, {});
  recursive_body.terminate(0);

  beast::SubroutineLibrary library;
  library.push_back({recursive_body.getData(), /*input_arity=*/0, /*output_arity=*/1,
                     /*max_steps_per_call=*/50});

  const auto result = runCaller(library, /*subroutine_id=*/0, /*input_arity=*/0,
                                /*output_arity=*/1, {});
  REQUIRE_FALSE(result.exited_abnormally);
  REQUIRE(result.output_value == 0);
}
