// Tier 3 acceptance: per-opcode parity sweep for the GPU device VM.
//
// Each TEST_CASE constructs a tiny hand-built program that exercises ONE GPU opcode
// (or a small family of related opcodes) in isolation, scores it on both the CPU and
// the GPU SHA-256 evaluator paths, and asserts the scores agree within float
// tolerance.
//
// We deliberately use `Sha256RoundEvaluator` as the test vehicle even for non-SHA
// opcodes: the evaluator's scoring function maps "what's in output vars 11..18" to a
// score that diverges visibly on any per-opcode bug, and reusing the existing host
// plumbing means we don't have to stand up a parallel "generic GPU runner". The cost
// is that each parity assertion is INDIRECT (a score difference, not a value
// difference); the upside is that the test runs in seconds without new fixtures.
//
// Skipped at runtime when no CUDA device is available (CI hosts without a GPU).

// Standard
#include <cmath>
#include <vector>

// Third-party
#include <catch2/catch.hpp>

// Internal
#include <beast/cpu_virtual_machine.hpp>
#include <beast/cuda/cuda_sha256_round_evaluator.hpp>
#include <beast/evaluators/sha256_round_evaluator.hpp>
#include <beast/program.hpp>
#include <beast/vm_session.hpp>

namespace {

constexpr uint32_t kVariableCount = 32;
constexpr uint32_t kStringTableCount = 0;
constexpr uint32_t kMaxStringSize = 0;

// Common evaluator config -- one trial, fixed K, short step budget. Per-opcode tests
// don't need many trials; one is enough to catch a parity divergence (every divergent
// opcode flips the same output vars the same way every trial).
beast::Sha256RoundEvaluator defaultConfig() {
  return beast::Sha256RoundEvaluator(/*trial_count=*/2, /*round_constant_index=*/0,
                                     /*max_steps_per_trial=*/400);
}

double scoreOnCpu(const std::vector<unsigned char>& genome,
                  const beast::Sha256RoundEvaluator& config) {
  beast::VmSession session(beast::Program(genome), kVariableCount, kStringTableCount,
                           kMaxStringSize);
  beast::Sha256RoundEvaluator eval(config.getTrialCount(), config.getRoundConstantIndex(),
                                   config.getMaxStepsPerTrial(),
                                   config.getRoundConstantsMode(),
                                   config.getRoundsPerTrial());
  return eval.evaluate(session);
}

double scoreOnGpu(const std::vector<unsigned char>& genome,
                  const beast::Sha256RoundEvaluator& config) {
  beast::cuda::CudaSha256RoundEvaluator gpu(config, kVariableCount);
  const std::vector<uint8_t> bytes(genome.begin(), genome.end());
  return gpu.evaluateOne(bytes);
}

void requireParity(const std::vector<unsigned char>& genome,
                   const beast::Sha256RoundEvaluator& config) {
  const double cpu = scoreOnCpu(genome, config);
  const double gpu = scoreOnGpu(genome, config);
  INFO("CPU score = " << cpu << ", GPU score = " << gpu);
  // 1e-4 matches the existing parity test; GPU does the per-trial reduction in
  // single precision, so we accept a tiny float drift but anything else flags a
  // semantic divergence.
  REQUIRE(std::abs(cpu - gpu) < 1e-4);
}

} // namespace

// ============================================================================
// Stack opcodes -- push var / push const / pop var / pop top / check empty.
// Builds a 4-byte stack at var 20 (size word) + slots 21..24.
// ============================================================================

TEST_CASE("cuda_opcode_parity_stack_push_pop", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  beast::Program prog(/*space=*/256);
  // Stack base at var 20. Push K (var 8), then pop into output 11.
  // CPU requires variables to be declared before use; GPU is more permissive
  // (it clamps to the register file). The declares are no-ops on GPU but mandatory
  // for the CPU side to not throw OutOfRange and short-circuit the score to 0.
  prog.declareVariable(20, beast::Program::VariableType::Int32);
  prog.declareVariable(21, beast::Program::VariableType::Int32);
  // NB: Program::setVariable's signature is (var_index, content, follow_links) --
  // the boolean is the LAST argument, not the second. Swapping them silently sets
  // the stack pointer to 1 and writes go to var 22 (undeclared -> CPU throws).
  prog.setVariable(/*var=*/20, /*content=*/0, /*follow_links=*/true); // stack size = 0
  prog.pushVariableOnStack(20, true, 8, true); // push vars[8] (= K)
  prog.popVariableFromStack(20, true, 11, true); // pop into output a
  prog.terminate(0);
  requireParity(prog.getData(), defaultConfig());
}

TEST_CASE("cuda_opcode_parity_stack_push_const_check_empty", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  beast::Program prog(/*space=*/256);
  prog.declareVariable(20, beast::Program::VariableType::Int32);
  prog.declareVariable(21, beast::Program::VariableType::Int32);
  prog.setVariable(/*var=*/20, /*content=*/0, /*follow_links=*/true);
  prog.pushConstantOnStack(20, true, 0xCAFEBABE);
  prog.checkIfStackIsEmpty(20, true, 11, true); // vars[11] = 0 (not empty)
  prog.popTopItemFromStack(20, true);
  prog.checkIfStackIsEmpty(20, true, 12, true); // vars[12] = 1 (empty)
  prog.terminate(0);
  requireParity(prog.getData(), defaultConfig());
}

// ============================================================================
// Declare / undeclare -- the bitmap toggle.
// CPU semantics: undeclaring a variable makes future reads / writes throw, which the
// evaluator catches and treats as "do nothing". Our GPU implementation skips the op,
// which produces the same observable output as long as the program doesn't depend on
// the abnormal-exit flag.
// ============================================================================

TEST_CASE("cuda_opcode_parity_declare_undeclare", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  beast::Program prog(/*space=*/256);
  prog.declareVariable(20, beast::Program::VariableType::Int32);
  prog.setVariable(/*var=*/20, /*content=*/0xDEADBEEF, /*follow_links=*/true);
  prog.copyVariable(20, true, 11, true); // var 11 (output a) = 0xDEADBEEF
  prog.undeclareVariable(20);
  // After undeclare, reading var 20 panics on CPU -> evaluator catches -> score 0
  // for the rest of the trial. We deliberately don't touch var 20 again. Write
  // another output to make the post-undeclare state observable in both backends.
  prog.setVariable(/*var=*/12, /*content=*/0x12345678, /*follow_links=*/true); // var 12 is pre-declared as output
  prog.terminate(0);
  requireParity(prog.getData(), defaultConfig());
}

// ============================================================================
// I/O reflection -- masks are uniform across launch.
// ============================================================================

TEST_CASE("cuda_opcode_parity_load_input_output_count", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  beast::Program prog(/*space=*/128);
  prog.loadInputCountIntoVariable(11, true); // SHA evaluator: 12 inputs
  prog.loadOutputCountIntoVariable(12, true); // SHA evaluator: 8 outputs
  prog.checkIfVariableIsInput(8, true, 13, true); // var 8 is input (K)
  prog.checkIfVariableIsOutput(11, true, 14, true); // var 11 is output (new_a)
  prog.terminate(0);
  requireParity(prog.getData(), defaultConfig());
}

// ============================================================================
// Variable-target jumps.
// Uses an absolute jump to skip an instruction that would overwrite the output.
// ============================================================================

TEST_CASE("cuda_opcode_parity_variable_target_unconditional_jump", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  // Test the GPU's byte_to_insn runtime lookup against the CPU's direct byte-PC
  // jump. We jump FORWARD over a terminate() to land on the "real" outputs --
  // jumping backwards is hard because we'd re-execute declareVariable(25) which
  // throws on CPU (already declared) and the evaluator catches -> score 0,
  // diverging from the GPU which makes redeclaration idempotent.
  //
  // Layout (byte-wise, fixed by Program::appendCodeX widths):
  //   declareVariable(25):                     6 bytes      [0..5]
  //   setVariable(25, content=jump_target):   10 bytes      [6..15]
  //   unconditionalJumpToAbsVarAddr(25):       6 bytes      [16..21]
  //   terminate(0):                            2 bytes      [22..23]   <-- skipped
  //   copyVariable(8 -> 11):                  10 bytes      [24..33]   <-- lands here
  //   copyVariable(9 -> 12):                  10 bytes      [34..43]
  //   terminate(0):                            2 bytes      [44..45]
  beast::Program prog(/*space=*/64);
  prog.declareVariable(25, beast::Program::VariableType::Int32);
  prog.setVariable(/*var=*/25, /*content=*/24, /*follow_links=*/true);
  prog.unconditionalJumpToAbsoluteVariableAddress(25, true);
  prog.terminate(0);
  prog.copyVariable(8, true, 11, true);
  prog.copyVariable(9, true, 12, true);
  prog.terminate(0);
  requireParity(prog.getData(), defaultConfig());
}

// ============================================================================
// Random -- CPU uses mt19937, GPU uses curand. Different generators produce
// different bit streams, so we DON'T expect bit-exact parity for random opcodes.
// What we DO require is "the program completes without divergent control flow", so
// we just exercise the path and assert the GPU doesn't crash or hang. This is the
// same trade-off the CPU evaluator's outer try/catch makes for randomness-driven
// programs.
// ============================================================================

TEST_CASE("cuda_opcode_parity_random_does_not_crash", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  beast::Program prog(/*space=*/64);
  prog.loadRandomValueIntoVariable(11, true);
  prog.terminate(0);
  // Just run; both backends must return a finite score.
  const auto config = defaultConfig();
  const double cpu = scoreOnCpu(prog.getData(), config);
  const double gpu = scoreOnGpu(prog.getData(), config);
  REQUIRE(std::isfinite(cpu));
  REQUIRE(std::isfinite(gpu));
  // The scores will diverge because the two RNGs are different, but both must be
  // in [0, 1] -- a bad parity bug would produce NaN or out-of-range values.
  REQUIRE(cpu >= 0.0);
  REQUIRE(cpu <= 1.0);
  REQUIRE(gpu >= 0.0);
  REQUIRE(gpu <= 1.0);
}

// ============================================================================
// Subroutines -- caller passes inputs, callee writes outputs back.
//
// We mount a tiny library with a single subroutine that copies input 0 to output 0
// (which in the callee's variable layout is var input_arity+1 = var 2). The caller
// program calls the subroutine and inspects the result.
// ============================================================================

TEST_CASE("cuda_opcode_parity_call_subroutine_copy", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  // Callee bytecode: copy var 0 (input) to var 2 (output for arity 1/1). Var 1 is
  // the trial-id slot per SubroutineEntry's documented layout.
  beast::Program callee(/*space=*/64);
  callee.copyVariable(0, true, 2, true);
  callee.terminate(0);

  // Caller bytecode: CallSubroutine(id=0, in=[var 8 -> input], out=[var 11 -> output]).
  beast::Program caller(/*space=*/64);
  // Manually invoke the variadic callSubroutine builder. SubroutineArgument vectors:
  const std::vector<beast::Program::SubroutineArgument> inputs{{8, true}};
  const std::vector<beast::Program::SubroutineArgument> outputs{{11, true}};
  caller.callSubroutine(0, inputs, outputs);
  caller.terminate(0);

  // Mount the library on BOTH backends.
  auto library = std::make_shared<beast::SubroutineLibrary>();
  library->push_back({std::vector<unsigned char>(callee.getData().begin(), callee.getData().end()),
                      /*input_arity=*/1, /*output_arity=*/1, /*max_steps_per_call=*/64});

  // CPU side
  const auto config = defaultConfig();
  beast::VmSession session(beast::Program(caller.getData()), kVariableCount,
                           kStringTableCount, kMaxStringSize);
  session.setSubroutineLibrary(
      std::const_pointer_cast<const beast::SubroutineLibrary>(library));
  beast::Sha256RoundEvaluator cpu_eval(config.getTrialCount(), config.getRoundConstantIndex(),
                                       config.getMaxStepsPerTrial(),
                                       config.getRoundConstantsMode(),
                                       config.getRoundsPerTrial());
  const double cpu = cpu_eval.evaluate(session);

  // GPU side
  beast::cuda::CudaSha256RoundEvaluator gpu(config, kVariableCount);
  gpu.setSubroutineLibrary(library);
  const double gpu_score = gpu.evaluateOne(
      std::vector<uint8_t>(caller.getData().begin(), caller.getData().end()));

  INFO("CPU = " << cpu << ", GPU = " << gpu_score);
  REQUIRE(std::abs(cpu - gpu_score) < 1e-4);
}

// ============================================================================
// Print* / PerformSystemCall -- DeviceNoOpEquivalent. These ops affect a host-only
// side-effect (panic log) that scoring never reads. The GPU folds them to NoOp; the
// CPU evaluator's outer try/catch makes them invisible to the score. Parity must
// hold even though the "side effects" are different.
// ============================================================================

TEST_CASE("cuda_opcode_parity_noop_equivalents", "[cuda][parity]") {
  if (!beast::cuda::isCudaAvailable()) {
    WARN("No CUDA device available; skipping parity check");
    return;
  }
  beast::Program prog(/*space=*/128);
  prog.copyVariable(8, true, 11, true);    // observable: output a = K
  prog.printVariable(8, true, false);      // host-only side effect (numeric print)
  prog.terminate(0);
  requireParity(prog.getData(), defaultConfig());
}
