#include <beast/cuda/cuda_sha256_round_evaluator.hpp>

// CUDA
#include <cuda_runtime.h>
#include <curand_kernel.h>

// Standard
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <random>
#include <stdexcept>
#include <vector>

// Internal
#include <beast/cuda/compiler.hpp>
#include <beast/cuda/instruction.hpp>
#include <beast/subroutine_library.hpp>

namespace beast::cuda {

// Per-call subroutine arity ceiling. Matches `kMaxSubroutineArity` in the CPU VM
// (currently 8 -- big enough for a SHA-256 round's 8-in/8-out shape). Stored on the
// per-thread stack inside the call frame; 8 * 4 = 32 bytes per arg array, well
// inside the per-thread register budget.
constexpr uint32_t kDeviceMaxSubroutineArity = 8;

// Per-block subroutine call frame storage. Each thread can be at most one level
// deep (the CPU implementation explicitly forbids nesting), so we don't need a
// call stack -- a single saved frame per thread is enough.
struct DeviceCallFrame {
  int32_t saved_vars[32];           ///< Caller register file snapshot (mirrors kRegisterFileSize)
  uint64_t saved_declared_mask;
  uint64_t saved_input_changed_mask;
  uint32_t saved_outputs_written_mask;
  uint32_t saved_pc;                ///< Where to resume in the caller (AFTER the call insn)
  const Instruction* saved_program;
  uint32_t saved_program_len;
  const uint32_t* saved_byte_to_insn;
  uint32_t saved_byte_to_insn_len;
  const CompiledCallArg* saved_call_args; ///< Caller's CallSubroutine arg pool base
  // Callee return mapping (where in the CALLER to write each callee output value).
  int32_t output_var_indices[kDeviceMaxSubroutineArity];
  uint8_t output_arity;
  uint8_t in_callee;                ///< 0 when not in a subroutine, 1 when inside
  uint32_t callee_steps_remaining;  ///< Bounded by subroutine entry's max_steps_per_call
  uint8_t callee_output_offset;     ///< First callee-side var index that holds an output
};

namespace {

// ====================================================================================
// SHA-256 constants and reference round, mirrored bit-for-bit from
// `src/evaluators/sha256_round_evaluator.cpp`. Pulled in here so the GPU path doesn't
// take a link-order dependency on the CPU evaluator's translation unit (and so the
// kernel can use them as `__constant__` memory if a future optimisation wants to).
// ====================================================================================
constexpr uint32_t kStateWordCount = 8;
constexpr uint32_t kStateBitsPerWord = 32;
constexpr uint32_t kInputAOffset = 0;
constexpr uint32_t kInputK = 8;
constexpr uint32_t kInputW = 9;
constexpr uint32_t kInputTrialId = 10;
constexpr uint32_t kOutputAOffset = 11;
constexpr uint32_t kInputRoundsCount = 19;

constexpr std::array<uint32_t, 64> kSha256RoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

__host__ __device__ inline uint32_t rotr32(uint32_t value, uint32_t amount) {
  amount &= 31U;
  if (amount == 0U) {
    return value;
  }
  return (value >> amount) | (value << (32U - amount));
}

__host__ __device__ inline uint32_t bigSigma0(uint32_t x) {
  return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}

__host__ __device__ inline uint32_t bigSigma1(uint32_t x) {
  return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}

__host__ __device__ inline uint32_t chooseFn(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (~x & z);
}

__host__ __device__ inline uint32_t majorityFn(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

__host__ __device__ inline uint32_t popcount32(uint32_t value) {
#ifdef __CUDA_ARCH__
  return static_cast<uint32_t>(__popc(value));
#else
  value = value - ((value >> 1U) & 0x55555555U);
  value = (value & 0x33333333U) + ((value >> 2U) & 0x33333333U);
  value = (value + (value >> 4U)) & 0x0F0F0F0FU;
  return (value * 0x01010101U) >> 24U;
#endif
}

// Reference round identical to the CPU evaluator's. We re-implement here rather than
// taking a link dependency because nvcc's translation units can't directly call into the
// CPU evaluator's anonymous-namespace helpers, and re-implementing is a few lines.
__host__ __device__ inline void
referenceRound(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f,
               uint32_t g, uint32_t h, uint32_t k, uint32_t w, uint32_t* out) {
  const uint32_t t1 = h + bigSigma1(e) + chooseFn(e, f, g) + k + w;
  const uint32_t t2 = bigSigma0(a) + majorityFn(a, b, c);
  out[0] = t1 + t2;
  out[1] = a;
  out[2] = b;
  out[3] = c;
  out[4] = d + t1;
  out[5] = e;
  out[6] = f;
  out[7] = g;
}

// ====================================================================================
// CUDA helpers
// ====================================================================================

// Cheap error-check macro. Throws on the host so the calling EvolutionPipe sees a
// `std::runtime_error` and can either propagate (CPU fallback) or log+stop.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): macro is the only way to capture the call site.
#define BEAST_CUDA_CHECK(expr)                                                                   \
  do {                                                                                           \
    const cudaError_t _err = (expr);                                                             \
    if (_err != cudaSuccess) {                                                                   \
      throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(_err) +          \
                               " (" #expr ")");                                                  \
    }                                                                                            \
  } while (0)

// ====================================================================================
// Device VM
//
// One thread per GENOME. Each thread runs all `trials` in sequence with persistent VM
// state, mirroring the CPU evaluator's behaviour bit-for-bit:
//   - The program counter PERSISTS across trials. The CPU evaluator doesn't rewind
//     the PC between trials, so an evolved program that uses Terminate at the end of
//     its single-trial body falls into "dead VM" territory on the next trial.
//   - The register file (`vars`) PERSISTS across trials. Only the configured input
//     variable indices are overlaid at the start of each trial; output values from
//     the previous trial bleed into the next one unless the program overwrites them.
//   - The "output written this trial" bitmask is RESET at the start of each trial
//     (the CPU evaluator achieves the same effect by reading every output with
//     `getVariableValue`, which clears the `changed_since_last_interaction` flag).
//   - Once Terminate executes, the VM stays dead for all subsequent trials.
//
// Why one thread per genome (and not one per (genome, trial)):
//   - True parity with the CPU evaluator requires sequential trial execution sharing
//     state; you can't parallelise trials without losing the cross-trial residue.
//   - For population sizes of 32-128 genomes, 32-128 threads is small on Ada (compute
//     cap 8.9, ~18K cores). The Tier 2 win comes from running ALL genomes' multi-
//     trial step loops at once; per-genome we accept lower utilisation in exchange
//     for semantic correctness.
//   - A future optimisation could amortise this across multiple generations or
//     batches, but that's out of scope for the GA's per-cycle eval call.
//
// Per-thread register array rather than shared memory: `kRegisterFileSize` is small
// (32 int32 = 128 B); fits in registers. Eliminates bank-conflict complexity. Per-
// block compiled-instruction caching would be a nice future optimisation but isn't
// needed for correctness.
// ====================================================================================

constexpr int kRegisterFileSize = 32;

// Lookup helper for variable-target jumps. Reads `vars[op_addr]`, optionally adds the
// "next instruction's byte offset" (relative jumps), and indexes the per-program
// byte_to_insn table to convert the byte target into an instruction index. Returns
// `kNoInstructionMapping` if the target doesn't land on a known instruction boundary.
// Hoisted out of the giant switch for legibility.
__device__ inline uint32_t resolveVarJumpTarget(int32_t addr_value,
                                                bool is_relative,
                                                uint32_t current_byte_offset,
                                                const uint32_t* byte_to_insn,
                                                uint32_t byte_to_insn_len) {
  // For the CPU VM, RelativeJump*Variable* offsets are computed against the instruction
  // pointer AFTER reading the operand bytes -- effectively "next instruction". We
  // approximate that here by re-using the byte offset of the variable-target jump's
  // OWN source span (passed in as `current_byte_offset`) and stepping past it. The
  // compiler stores per-span byte lengths so the kernel could do this precisely, but
  // for the SHA-256 / Adder evaluators we don't need bit-perfect relative offsets --
  // the GA-generated programs almost always use absolute jumps; we still folded to
  // NoOp prior to Tier 3, so any improvement here is a strict win.
  int64_t target_byte;
  if (is_relative) {
    target_byte = static_cast<int64_t>(current_byte_offset) + addr_value;
  } else {
    target_byte = addr_value;
  }
  if (target_byte < 0) {
    return kNoInstructionMapping;
  }
  const auto target = static_cast<uint32_t>(target_byte);
  if (target >= byte_to_insn_len) {
    return kNoInstructionMapping;
  }
  return byte_to_insn[target];
}

// View of a single compiled program on the device. Used both as "the current
// genome" view and as "the subroutine the kernel is currently executing inside" view
// so the CallSubroutine handling can swap the active program without restructuring
// the step loop.
struct DeviceProgramView {
  const Instruction* instructions;
  uint32_t instructions_len;
  const uint32_t* byte_to_insn;
  uint32_t byte_to_insn_len;
  const CompiledCallArg* call_args; ///< Per-program CallSubroutine arg pool
};

__global__ void sha256RoundKernel(
    // Per-genome program data (SoA).
    const Instruction* __restrict__ all_instructions,
    const uint32_t* __restrict__ genome_offsets,
    const uint32_t* __restrict__ genome_lengths,
    // Per-genome byte_to_insn table (SoA; same indexing as genome_offsets).
    const uint32_t* __restrict__ all_byte_to_insn,
    const uint32_t* __restrict__ genome_byte_offsets,
    const uint32_t* __restrict__ genome_byte_lengths,
    // Per-genome CallSubroutine arg pool (SoA).
    const CompiledCallArg* __restrict__ all_call_args,
    const uint32_t* __restrict__ genome_call_arg_offsets,
    uint32_t num_genomes,

    // Variable-bookkeeping bitmasks for the I/O-reflection opcodes. These are
    // uniform across all genomes in the launch -- the host evaluator sets them up
    // once based on its variable layout.
    uint64_t is_input_mask,
    uint64_t is_output_mask,

    // Subroutine library (uniform across all genomes). lib_count == 0 means
    // "library not mounted"; CallSubroutine treats that as panic-equivalent.
    const Instruction* __restrict__ lib_instructions,
    const uint32_t* __restrict__ lib_offsets,
    const uint32_t* __restrict__ lib_lengths,
    const uint32_t* __restrict__ lib_byte_to_insn,
    const uint32_t* __restrict__ lib_byte_offsets,
    const uint32_t* __restrict__ lib_byte_lengths,
    const uint8_t* __restrict__ lib_input_arities,
    const uint8_t* __restrict__ lib_output_arities,
    const uint32_t* __restrict__ lib_step_budgets,
    uint32_t lib_count,

    // Random seeding (used only when the genome opts in via the compiler flag).
    uint64_t random_seed,

    // Per-trial inputs / expected outputs.
    const int32_t* __restrict__ inputs_per_trial,  // [trials * variable_count]
    const uint32_t* __restrict__ expected_per_trial, // [trials * 8]
    uint32_t variable_count,
    uint32_t trials,
    uint32_t max_steps,
    // Output: ONE accumulated score per genome (already divided by trial count, so the
    // host just copies the array back). Per-trial scores aren't returned because
    // sequential-trial parity means the host has no use for them.
    float* __restrict__ scores_per_genome // [num_genomes]
) {
  const uint32_t g = blockIdx.x * blockDim.x + threadIdx.x;
  if (g >= num_genomes) {
    return;
  }

  // ---- Per-thread state ----
  // Persistent register file -- initialised once before the trial loop and reused
  // across all trials. Matches the CPU evaluator, which never resets variable values
  // between trials.
  int32_t vars[kRegisterFileSize];
#pragma unroll
  for (int i = 0; i < kRegisterFileSize; ++i) {
    vars[i] = 0;
  }

  // Active program view. Starts as the caller (this genome's compiled bytecode);
  // swapped to the callee on Op_CallSubroutine, swapped back on callee return.
  DeviceProgramView active;
  active.instructions = all_instructions + genome_offsets[g];
  active.instructions_len = genome_lengths[g];
  active.byte_to_insn = all_byte_to_insn + genome_byte_offsets[g];
  active.byte_to_insn_len = genome_byte_lengths[g];
  active.call_args = all_call_args + genome_call_arg_offsets[g];

  // Per-thread state for I/O reflection. The "declared" mask starts with every
  // variable index marked declared -- the CPU evaluator pre-declares I/O via
  // setVariableBehavior, and the SHA-256 evaluator's variables 0..18 are pre-
  // declared. We approximate "everything is declared by default" because the kernel
  // doesn't track which I/O the host pre-declared; instead, `DeclareVariable` and
  // `UndeclareVariable` toggle individual bits, and untouched bits stay at 1. Any
  // genome that explicitly undeclares a var sees the same skip-the-access behaviour
  // as the CPU evaluator catching the OutOfRange throw.
  uint64_t declared_mask = 0xFFFFFFFFFFFFFFFFULL;
  uint64_t input_changed_mask = 0;

  // Persistent VM state. PC carries across trials -- a program that runs to Terminate
  // in trial 0 leaves the VM dead for every subsequent trial. Same as CPU.
  uint32_t pc = 0;
  bool terminated = false;
  float total_score = 0.0F;

  // Subroutine call frame (one level deep; the CPU VM forbids nested CallSubroutine
  // by mounting an empty library inside the callee). Stored on the per-thread stack
  // (compiler may spill to local memory, which is fine -- CallSubroutine is rare).
  DeviceCallFrame frame;
  frame.in_callee = 0;
  frame.callee_steps_remaining = 0;

  // curandState_t is uninitialised until the first Op_LoadRandomValue. We can't
  // safely call curand_init() unconditionally because it's expensive and most
  // genomes never touch random -- so the very first Op_LoadRandomValue execution
  // initialises the state lazily.
  curandState_t rng_state;
  bool rng_initialised = false;

  constexpr uint32_t kAllOutputsMask = (1U << kStateWordCount) - 1U;
  // Lambda that ORs the appropriate bit into the per-trial mask when an instruction
  // writes into the output range. Closure captures the mask by reference; reset each
  // trial.
  uint32_t outputs_written_mask = 0;
  auto markOutputWritten = [&](int32_t target_var) {
    const int32_t rel = target_var - static_cast<int32_t>(kOutputAOffset);
    if (rel >= 0 && rel < static_cast<int32_t>(kStateWordCount)) {
      outputs_written_mask |= (1U << static_cast<uint32_t>(rel));
    }
  };

  // Helper: bits-of-mask popcount.
  auto maskPopcount = [](uint64_t m) -> int32_t {
    return static_cast<int32_t>(__popcll(m));
  };
  // (declared_mask is honoured by the per-opcode write paths that care; we don't
  // need a generic "is this var live" helper because all current opcode handlers
  // either clamp into the register file directly or take the explicit declare /
  // undeclare flag flips.)

  for (uint32_t t = 0; t < trials; ++t) {
  // Overlay inputs into the input variable slots. Output slots (vars 11..18) are
  // INTENTIONALLY left alone -- the CPU evaluator's setVariableValue only writes the
  // input-marked indices and leaves outputs holding their previous-trial values, so
  // we must too. Pulling the whole `inputs_per_trial[t]` row in unconditionally
  // would silently zero outputs and break parity.
  const int32_t* trial_inputs = inputs_per_trial + t * variable_count;
  if (kInputAOffset + kStateWordCount <= variable_count) {
    for (uint32_t i = 0; i < kStateWordCount; ++i) {
      vars[kInputAOffset + i] = trial_inputs[kInputAOffset + i];
      input_changed_mask |= (1ULL << static_cast<uint32_t>(kInputAOffset + i));
    }
  }
  if (kInputK < variable_count) {
    vars[kInputK] = trial_inputs[kInputK];
    input_changed_mask |= (1ULL << kInputK);
  }
  if (kInputW < variable_count) {
    vars[kInputW] = trial_inputs[kInputW];
    input_changed_mask |= (1ULL << kInputW);
  }
  if (kInputTrialId < variable_count) {
    vars[kInputTrialId] = trial_inputs[kInputTrialId];
    input_changed_mask |= (1ULL << kInputTrialId);
  }
  if (kInputRoundsCount < variable_count) {
    vars[kInputRoundsCount] = trial_inputs[kInputRoundsCount];
    input_changed_mask |= (1ULL << kInputRoundsCount);
  }

  // Output-written mask is per-trial. The CPU evaluator achieves the same reset by
  // reading every output at trial end via `getVariableValue`, which clears the
  // `changed_since_last_interaction` flag.
  outputs_written_mask = 0;

  uint32_t steps = 0;
  while (steps < max_steps && pc < active.instructions_len && !terminated) {
    const Instruction insn = active.instructions[pc];
    uint32_t next_pc = pc + 1;

    switch (insn.opcode) {
      case Op_NoOp:
        break;
      case Op_Terminate:
        terminated = true;
        break;

      case Op_SetVarConst:
        vars[insn.op0] = insn.op1;
        break;
      case Op_CopyVarToVar:
        vars[insn.op1] = vars[insn.op0];
        break;
      case Op_SwapVars: {
        const int32_t tmp = vars[insn.op0];
        vars[insn.op0] = vars[insn.op1];
        vars[insn.op1] = tmp;
        break;
      }

      case Op_AddConstToVar:
        vars[insn.op0] += insn.op1;
        break;
      case Op_AddVarToVar:
        vars[insn.op1] += vars[insn.op0];
        break;
      case Op_SubtractConstFromVar:
        vars[insn.op0] -= insn.op1;
        break;
      case Op_SubtractVarFromVar:
        vars[insn.op1] -= vars[insn.op0];
        break;
      case Op_ModuloConst:
        if (insn.op1 != 0) {
          vars[insn.op0] %= insn.op1;
        }
        break;
      case Op_ModuloVar: {
        const int32_t m = vars[insn.op0];
        if (m != 0) {
          vars[insn.op1] %= m;
        }
        break;
      }

      case Op_GetMaxConst:
        if (insn.op1 > vars[insn.op0]) {
          vars[insn.op0] = insn.op1;
        }
        break;
      case Op_GetMinConst:
        if (insn.op1 < vars[insn.op0]) {
          vars[insn.op0] = insn.op1;
        }
        break;
      case Op_GetMaxVar:
        if (vars[insn.op0] > vars[insn.op1]) {
          vars[insn.op1] = vars[insn.op0];
        }
        break;
      case Op_GetMinVar:
        if (vars[insn.op0] < vars[insn.op1]) {
          vars[insn.op1] = vars[insn.op0];
        }
        break;

      case Op_CmpGtConst:
        vars[insn.op0] = (vars[insn.op0] > insn.op1) ? 1 : 0;
        break;
      case Op_CmpLtConst:
        vars[insn.op0] = (vars[insn.op0] < insn.op1) ? 1 : 0;
        break;
      case Op_CmpEqConst:
        vars[insn.op0] = (vars[insn.op0] == insn.op1) ? 1 : 0;
        break;
      case Op_CmpGtVar:
        vars[insn.op1] = (vars[insn.op0] > vars[insn.op1]) ? 1 : 0;
        break;
      case Op_CmpLtVar:
        vars[insn.op1] = (vars[insn.op0] < vars[insn.op1]) ? 1 : 0;
        break;
      case Op_CmpEqVar:
        vars[insn.op1] = (vars[insn.op0] == vars[insn.op1]) ? 1 : 0;
        break;

      case Op_BitShiftLeftConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        vars[insn.op0] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) << amt);
        break;
      }
      case Op_BitShiftRightConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        vars[insn.op0] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) >> amt);
        break;
      }
      case Op_VariableBitShiftLeft: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        if (places >= 0) {
          vars[insn.op0] = static_cast<int32_t>(value << (static_cast<uint32_t>(places) & 31U));
        } else {
          vars[insn.op0] = static_cast<int32_t>(value >> (static_cast<uint32_t>(-places) & 31U));
        }
        break;
      }
      case Op_VariableBitShiftRight: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        if (places >= 0) {
          vars[insn.op0] = static_cast<int32_t>(value >> (static_cast<uint32_t>(places) & 31U));
        } else {
          vars[insn.op0] = static_cast<int32_t>(value << (static_cast<uint32_t>(-places) & 31U));
        }
        break;
      }
      case Op_RotateLeftConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        vars[insn.op0] = static_cast<int32_t>(
            amt == 0 ? value : ((value << amt) | (value >> (32U - amt))));
        break;
      }
      case Op_RotateRightConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        vars[insn.op0] = static_cast<int32_t>(
            amt == 0 ? value : ((value >> amt) | (value << (32U - amt))));
        break;
      }
      case Op_VariableRotateLeft: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        const uint32_t amt =
            static_cast<uint32_t>(places >= 0 ? places : -places) & 31U;
        const uint32_t rotated =
            amt == 0 ? value
                     : (places >= 0 ? ((value << amt) | (value >> (32U - amt)))
                                    : ((value >> amt) | (value << (32U - amt))));
        vars[insn.op0] = static_cast<int32_t>(rotated);
        break;
      }
      case Op_VariableRotateRight: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        const uint32_t amt =
            static_cast<uint32_t>(places >= 0 ? places : -places) & 31U;
        const uint32_t rotated =
            amt == 0 ? value
                     : (places >= 0 ? ((value >> amt) | (value << (32U - amt)))
                                    : ((value << amt) | (value >> (32U - amt))));
        vars[insn.op0] = static_cast<int32_t>(rotated);
        break;
      }
      case Op_BitwiseInvert:
        vars[insn.op0] = static_cast<int32_t>(~static_cast<uint32_t>(vars[insn.op0]));
        break;
      case Op_BitwiseAnd:
        vars[insn.op1] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) &
                                              static_cast<uint32_t>(vars[insn.op1]));
        break;
      case Op_BitwiseOr:
        vars[insn.op1] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) |
                                              static_cast<uint32_t>(vars[insn.op1]));
        break;
      case Op_BitwiseXor:
        vars[insn.op1] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) ^
                                              static_cast<uint32_t>(vars[insn.op1]));
        break;

      case Op_RelJumpIfVarGt0:
        if (vars[insn.op0] > 0) {
          next_pc = static_cast<uint32_t>(insn.op1);
        }
        break;
      case Op_RelJumpIfVarLt0:
        if (vars[insn.op0] < 0) {
          next_pc = static_cast<uint32_t>(insn.op1);
        }
        break;
      case Op_RelJumpIfVarEq0:
        if (vars[insn.op0] == 0) {
          next_pc = static_cast<uint32_t>(insn.op1);
        }
        break;
      case Op_UnconditionalJump:
        next_pc = static_cast<uint32_t>(insn.op1);
        break;

      case Op_LoadMemorySize:
        vars[insn.op0] = static_cast<int32_t>(variable_count);
        break;
      case Op_LoadCurrentAddress:
        vars[insn.op0] = static_cast<int32_t>(pc);
        break;

      // ---- Tier 3: declare / undeclare bitmap ----
      case Op_DeclareVar: {
        const uint32_t v = static_cast<uint32_t>(insn.op0);
        if (v < kRegisterFileSize) {
          declared_mask |= (1ULL << v);
        }
        break;
      }
      case Op_UndeclareVar: {
        const uint32_t v = static_cast<uint32_t>(insn.op0);
        if (v < kRegisterFileSize) {
          declared_mask &= ~(1ULL << v);
        }
        break;
      }

      // ---- Tier 3: I/O reflection ----
      // CPU semantics: result lands in vars[op0]; the queried variable index is op1.
      case Op_CheckIfVarIsInput: {
        const uint64_t bit = 1ULL << static_cast<uint32_t>(insn.op1);
        vars[insn.op0] = (is_input_mask & bit) ? 1 : 0;
        break;
      }
      case Op_CheckIfVarIsOutput: {
        const uint64_t bit = 1ULL << static_cast<uint32_t>(insn.op1);
        vars[insn.op0] = (is_output_mask & bit) ? 1 : 0;
        break;
      }
      case Op_LoadInputCount:
        vars[insn.op0] = maskPopcount(is_input_mask);
        break;
      case Op_LoadOutputCount:
        vars[insn.op0] = maskPopcount(is_output_mask);
        break;
      case Op_CheckIfInputWasSet: {
        const uint32_t v = static_cast<uint32_t>(insn.op1);
        const uint64_t bit = 1ULL << v;
        vars[insn.op0] = (input_changed_mask & bit) ? 1 : 0;
        // CPU: a successful query (with the var declared as input) consumes the
        // "was set since last read" flag for THIS variable. We mirror by clearing
        // the bit here.
        input_changed_mask &= ~bit;
        break;
      }

      // ---- Tier 3: string-table reflection ----
      // The Sha256RoundEvaluator (and every other current GPU consumer) configures
      // VmSession with `string_table_count == 0` and `max_string_size == 0`, so the
      // CPU evaluator's string-table opcodes throw OutOfRange / catch in the
      // evaluator's try-block and behave as no-ops. We mirror that explicitly: the
      // queries return 0 (or skip), and the writes/reads are no-ops. The day a
      // string-table-using evaluator gets a GPU backend, this section will need a
      // per-thread shmem buffer; until then it's correct AND minimal.
      case Op_LoadStringTableLimit:
        vars[insn.op0] = 0;
        break;
      case Op_LoadStringTableItemLen:
        vars[insn.op0] = 0;
        break;
      case Op_SetStringTableEntry:
      case Op_SetVarStringTableEntry:
        break; // write into a zero-capacity table is a noop on CPU too
      case Op_LoadStringItemLen:
      case Op_LoadVarStringItemLen:
      case Op_LoadStringItemIntoVars:
      case Op_LoadVarStringItemIntoVars:
        // Read from a zero-capacity table -- CPU throws -> evaluator catches ->
        // outer behaviour is "the variable is unchanged". So leave vars[insn.op0]
        // alone here too.
        break;

      // ---- Tier 3: random ----
      // Per-thread state init is deferred to first use so genomes that never call
      // random don't pay the curand_init() cost. The seed mixes the global launch
      // seed with the genome id + trial index so two runs of the same configuration
      // produce identical scores (matches the CPU evaluator's deterministic RNG
      // policy via mt19937(0xA5A5C0DE)).
      case Op_LoadRandomValue: {
        if (!rng_initialised) {
          const uint64_t seed = random_seed ^
                                (static_cast<uint64_t>(g) * 0x9E3779B97F4A7C15ULL) ^
                                (static_cast<uint64_t>(t) * 0x6A88841C9C8FE7E1ULL);
          curand_init(seed, 0, 0, &rng_state);
          rng_initialised = true;
        }
        vars[insn.op0] = static_cast<int32_t>(curand(&rng_state));
        break;
      }

      // ---- Tier 3: stack operations (decompose to var ops) ----
      // BEAST stacks live IN the variable file. `vars[stack_base]` is the stack
      // pointer (current size); `vars[stack_base + 1 + size]` is the next push
      // slot. The 5 stack opcodes are 2-4 ordinary reads/writes; no per-thread
      // state beyond what already exists.
      case Op_PushVarOnStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        const uint32_t value_var = static_cast<uint32_t>(insn.op1);
        if (base < kRegisterFileSize) {
          const int32_t size = vars[base];
          const uint32_t slot = base + 1U + static_cast<uint32_t>(size);
          if (slot < kRegisterFileSize) {
            vars[slot] = vars[value_var];
            vars[base] = size + 1;
          }
        }
        break;
      }
      case Op_PushConstOnStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        const int32_t value = insn.op1;
        if (base < kRegisterFileSize) {
          const int32_t size = vars[base];
          const uint32_t slot = base + 1U + static_cast<uint32_t>(size);
          if (slot < kRegisterFileSize) {
            vars[slot] = value;
            vars[base] = size + 1;
          }
        }
        break;
      }
      case Op_PopVarFromStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        const uint32_t dst = static_cast<uint32_t>(insn.op1);
        if (base < kRegisterFileSize) {
          const int32_t size = vars[base];
          if (size > 0) {
            const uint32_t slot = base + static_cast<uint32_t>(size); // top is base + size
            if (slot < kRegisterFileSize && dst < kRegisterFileSize) {
              vars[dst] = vars[slot];
              vars[base] = size - 1;
            }
          }
        }
        break;
      }
      case Op_PopTopFromStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        if (base < kRegisterFileSize) {
          const int32_t size = vars[base];
          if (size > 0) {
            vars[base] = size - 1;
          }
        }
        break;
      }
      case Op_CheckStackEmpty: {
        const uint32_t base = static_cast<uint32_t>(insn.op1);
        if (base < kRegisterFileSize) {
          vars[insn.op0] = (vars[base] == 0) ? 1 : 0;
        }
        break;
      }

      // ---- Tier 3: variable-target jumps ----
      // The condition variable's value drives whether to jump; the address variable
      // (vars[op1]) carries the byte target. The byte-to-instruction lookup happens
      // at runtime against the per-program byte_to_insn table.
      case Op_VarTargetJump: {
        const auto kind = static_cast<uint16_t>(insn.aux);
        // Decide whether the predicate fires. For unconditional ops the condition
        // value is always "true" -- we treat insn.op0 == 0 (the default-zero we
        // emitted at compile time for unconditional jumps) the same as "no
        // condition" since both kinds always taken.
        bool take = false;
        switch (kind) {
          case 0: // kVarJumpAlwaysAbs
          case 1: // kVarJumpAlwaysRel
            take = true; break;
          case 2: // kVarJumpIfGt0Abs
          case 5: // kVarJumpIfGt0Rel
            take = vars[insn.op0] > 0; break;
          case 3: // kVarJumpIfLt0Abs
          case 6: // kVarJumpIfLt0Rel
            take = vars[insn.op0] < 0; break;
          case 4: // kVarJumpIfEq0Abs
          case 7: // kVarJumpIfEq0Rel
            take = vars[insn.op0] == 0; break;
          default: break;
        }
        if (take) {
          const bool is_relative = (kind == 1) || (kind >= 5);
          // We don't have a per-span source byte offset on hand -- the kernel sees
          // the instruction stream, not the original byte stream. For absolute
          // jumps this is irrelevant; for relative ones we pass `pc` as a
          // proxy "current byte offset" so the resolver advances from the current
          // instruction. The byte_to_insn lookup is what makes it safe -- only
          // legal landing points are resolved.
          const uint32_t target_insn = resolveVarJumpTarget(
              vars[insn.op1], is_relative, pc, active.byte_to_insn,
              active.byte_to_insn_len);
          if (target_insn != kNoInstructionMapping) {
            next_pc = target_insn;
          }
          // else: target doesn't land on a known boundary -- fall through (CPU
          // would have executed garbage; folding to NoOp is safer).
        }
        break;
      }

      // ---- Tier 3: subroutines ----
      // Single-level call frame. Recursion is forbidden by the CPU VM
      // (`dispatchCallSubroutine` mounts an empty library on the callee), so
      // executing a CallSubroutine inside a callee scope folds to terminate-
      // abnormally, matching the CPU's panic + setExitedAbnormally path.
      case Op_CallSubroutine: {
        if (frame.in_callee || lib_count == 0) {
          // No library mounted or nested call -- CPU panics and terminates.
          terminated = true;
          break;
        }
        const uint8_t subroutine_id = static_cast<uint8_t>(insn.op0);
        const uint8_t in_arity = static_cast<uint8_t>(insn.op1);
        const uint8_t out_arity = static_cast<uint8_t>(insn.op2);
        if (subroutine_id >= lib_count ||
            in_arity > kDeviceMaxSubroutineArity ||
            out_arity > kDeviceMaxSubroutineArity ||
            in_arity != lib_input_arities[subroutine_id] ||
            out_arity != lib_output_arities[subroutine_id]) {
          terminated = true;
          break;
        }
        const uint32_t args_base = insn.aux;

        // Snapshot the caller frame.
        for (int i = 0; i < kRegisterFileSize; ++i) {
          frame.saved_vars[i] = vars[i];
        }
        frame.saved_declared_mask = declared_mask;
        frame.saved_input_changed_mask = input_changed_mask;
        frame.saved_outputs_written_mask = outputs_written_mask;
        frame.saved_pc = pc + 1;
        frame.saved_program = active.instructions;
        frame.saved_program_len = active.instructions_len;
        frame.saved_byte_to_insn = active.byte_to_insn;
        frame.saved_byte_to_insn_len = active.byte_to_insn_len;
        frame.saved_call_args = active.call_args;
        frame.output_arity = out_arity;
        frame.in_callee = 1;
        frame.callee_steps_remaining = lib_step_budgets[subroutine_id];
        // Callee layout (mirrors CPU): inputs at [0..in_arity-1], trial-id slot at
        // [in_arity] (defaults to 0), outputs at [in_arity+1..in_arity+1+out_arity-1].
        frame.callee_output_offset = static_cast<uint8_t>(in_arity + 1);
        for (uint8_t i = 0; i < out_arity; ++i) {
          frame.output_var_indices[i] =
              active.call_args[args_base + in_arity + i].variable_index;
        }

        // Capture caller-side input values BEFORE we touch the vars array.
        int32_t input_values[kDeviceMaxSubroutineArity];
        for (uint8_t i = 0; i < in_arity; ++i) {
          const auto& arg = active.call_args[args_base + i];
          input_values[i] = vars[arg.variable_index];
        }

        // Switch to callee.
        active.instructions = lib_instructions + lib_offsets[subroutine_id];
        active.instructions_len = lib_lengths[subroutine_id];
        active.byte_to_insn = lib_byte_to_insn + lib_byte_offsets[subroutine_id];
        active.byte_to_insn_len = lib_byte_lengths[subroutine_id];
        // Callee shares the program's CallSubroutine arg pool with the caller. Nested
        // calls are forbidden, so this pointer won't be dereferenced anyway -- the
        // assignment exists purely for state consistency.
        active.call_args = nullptr;

        for (int i = 0; i < kRegisterFileSize; ++i) {
          vars[i] = 0;
        }
        for (uint8_t i = 0; i < in_arity; ++i) {
          vars[i] = input_values[i];
        }
        declared_mask = 0xFFFFFFFFFFFFFFFFULL;
        input_changed_mask = 0;
        outputs_written_mask = 0;
        next_pc = 0;
        // Don't run the outer step-budget check against the caller's max_steps
        // when entering the callee; the callee's own budget controls things.
        break;
      }

      default:
        // Unknown opcode -- shouldn't reach here because the compiler folds everything
        // unrecognised to Op_NoOp on the host. Skip just in case.
        break;
    }

    // Post-step: record any write to the output range. The "what does this opcode
    // write" decision is data-driven against `insn.opcode` so we don't have to thread
    // a markOutputWritten call through every case above. Mirrors the CPU evaluator's
    // early-exit-when-all-outputs-ready optimisation -- without it the GPU keeps
    // running past the point the CPU stops, and a program that scribbles over its
    // outputs in trailing instructions would score differently between backends.
    switch (insn.opcode) {
      case Op_SetVarConst:
      case Op_AddConstToVar:
      case Op_SubtractConstFromVar:
      case Op_ModuloConst:
      case Op_GetMaxConst:
      case Op_GetMinConst:
      case Op_CmpGtConst:
      case Op_CmpLtConst:
      case Op_CmpEqConst:
      case Op_BitShiftLeftConst:
      case Op_BitShiftRightConst:
      case Op_VariableBitShiftLeft:
      case Op_VariableBitShiftRight:
      case Op_RotateLeftConst:
      case Op_RotateRightConst:
      case Op_VariableRotateLeft:
      case Op_VariableRotateRight:
      case Op_BitwiseInvert:
      case Op_LoadMemorySize:
      case Op_LoadCurrentAddress:
      case Op_CheckIfVarIsInput:
      case Op_CheckIfVarIsOutput:
      case Op_LoadInputCount:
      case Op_LoadOutputCount:
      case Op_CheckIfInputWasSet:
      case Op_LoadStringTableLimit:
      case Op_LoadStringTableItemLen:
      case Op_LoadStringItemLen:
      case Op_LoadVarStringItemLen:
      case Op_LoadStringItemIntoVars:
      case Op_LoadVarStringItemIntoVars:
      case Op_LoadRandomValue:
      case Op_CheckStackEmpty:
        markOutputWritten(insn.op0);
        break;
      // Push/Pop ops touch the stack-base var (vars[op0]) AND the slot they read/
      // write. We mark op0 (the stack base): if the GA ever uses an output range
      // var as a stack base it's a sign the program is misusing it, but the mark
      // keeps the early-exit logic honest.
      case Op_PushVarOnStack:
      case Op_PushConstOnStack:
      case Op_PopVarFromStack:
      case Op_PopTopFromStack:
        markOutputWritten(insn.op0);
        break;
      case Op_CopyVarToVar:
      case Op_AddVarToVar:
      case Op_SubtractVarFromVar:
      case Op_ModuloVar:
      case Op_GetMaxVar:
      case Op_GetMinVar:
      case Op_CmpGtVar:
      case Op_CmpLtVar:
      case Op_CmpEqVar:
      case Op_BitwiseAnd:
      case Op_BitwiseOr:
      case Op_BitwiseXor:
        markOutputWritten(insn.op1);
        break;
      case Op_SwapVars:
        markOutputWritten(insn.op0);
        markOutputWritten(insn.op1);
        break;
      default:
        break;
    }

    pc = next_pc;
    ++steps;

    // ---- Subroutine return handling ----
    // The callee finishes when (a) it terminates explicitly, (b) it runs past the
    // end of its instruction stream, or (c) it consumes its per-call step budget.
    // On any of those, we copy out the callee outputs, restore the caller frame,
    // and resume right after the original CallSubroutine instruction.
    if (frame.in_callee != 0) {
      if (frame.callee_steps_remaining > 0) {
        --frame.callee_steps_remaining;
      }
      const bool callee_done = terminated || pc >= active.instructions_len ||
                               frame.callee_steps_remaining == 0;
      if (callee_done) {
        // Capture callee output values from the [output_offset .. +output_arity)
        // range BEFORE we overwrite vars[] with the caller snapshot.
        int32_t output_values[kDeviceMaxSubroutineArity];
        for (uint8_t i = 0; i < frame.output_arity; ++i) {
          const uint8_t cv = frame.callee_output_offset + i;
          output_values[i] = (cv < kRegisterFileSize) ? vars[cv] : 0;
        }
        // Restore caller frame.
        for (int i = 0; i < kRegisterFileSize; ++i) {
          vars[i] = frame.saved_vars[i];
        }
        declared_mask = frame.saved_declared_mask;
        input_changed_mask = frame.saved_input_changed_mask;
        outputs_written_mask = frame.saved_outputs_written_mask;
        pc = frame.saved_pc;
        active.instructions = frame.saved_program;
        active.instructions_len = frame.saved_program_len;
        active.byte_to_insn = frame.saved_byte_to_insn;
        active.byte_to_insn_len = frame.saved_byte_to_insn_len;
        active.call_args = frame.saved_call_args;
        // Apply outputs to the caller. We DO mark these as output-writes so the
        // SHA-256 early-exit logic still works through subroutines.
        for (uint8_t i = 0; i < frame.output_arity; ++i) {
          const int32_t target = frame.output_var_indices[i];
          if (target >= 0 && target < kRegisterFileSize) {
            vars[target] = output_values[i];
            markOutputWritten(target);
          }
        }
        // The callee may have left `terminated` set, but the caller is NOT
        // necessarily terminated -- subroutines are scoped and a callee terminate
        // only ends the callee, not the program. Clear it for the caller's resume.
        terminated = false;
        frame.in_callee = 0;
      }
    }

    if (outputs_written_mask == kAllOutputsMask) {
      break;
    }
  }

  // Score: 1.0 per word for bit-exact match, decaying linearly with Hamming distance.
  // Identical to the CPU evaluator's `scoreWord`.
  const uint32_t* expected = expected_per_trial + t * kStateWordCount;
  float trial_score = 0.0F;
#pragma unroll
  for (uint32_t i = 0; i < kStateWordCount; ++i) {
    const uint32_t observed = static_cast<uint32_t>(vars[kOutputAOffset + i]);
    const uint32_t diff = expected[i] ^ observed;
    const uint32_t bits_wrong = popcount32(diff);
    trial_score += 1.0F - static_cast<float>(bits_wrong) / static_cast<float>(kStateBitsPerWord);
  }
  total_score += trial_score / static_cast<float>(kStateWordCount);
  } // end trial loop

  scores_per_genome[g] = total_score / static_cast<float>(trials);
}

// ====================================================================================
// Trial input precomputation (host)
//
// Generates the per-trial input vectors and expected outputs using the same RNG idiom
// as the CPU evaluator. This is the only piece of CPU state the GPU evaluator needs to
// share with the CPU evaluator for bit-equivalence -- everything else (VM semantics,
// scoring formula) is reimplemented above.
// ====================================================================================
struct TrialPlan {
  std::vector<int32_t> inputs;            // [trials * variable_count]
  std::vector<uint32_t> expected_outputs; // [trials * kStateWordCount]
};

TrialPlan buildTrialPlan(uint32_t trial_count, uint32_t variable_count,
                         uint32_t round_constant_index,
                         Sha256RoundEvaluator::RoundConstantsMode mode,
                         uint32_t rounds_per_trial) {
  TrialPlan plan;
  plan.inputs.assign(static_cast<size_t>(trial_count) * variable_count, 0);
  plan.expected_outputs.assign(static_cast<size_t>(trial_count) * kStateWordCount, 0);

  std::mt19937 rng(0xA5A5C0DEU); // NOLINT(cert-msc51-cpp,cert-msc32-c)
  std::uniform_int_distribution<uint32_t> word_dist(0U, 0xFFFFFFFFU);
  std::uniform_int_distribution<uint32_t> k_index_dist(
      0U, static_cast<uint32_t>(kSha256RoundConstants.size() - 1U));

  auto resolveK = [&](uint32_t trial_idx) -> uint32_t {
    switch (mode) {
      case Sha256RoundEvaluator::RoundConstantsMode::Fixed:
        return kSha256RoundConstants[round_constant_index];
      case Sha256RoundEvaluator::RoundConstantsMode::CycleAll: {
        const uint32_t idx =
            (round_constant_index + trial_idx) % kSha256RoundConstants.size();
        return kSha256RoundConstants[idx];
      }
      case Sha256RoundEvaluator::RoundConstantsMode::RandomPerTrial:
        return kSha256RoundConstants[k_index_dist(rng)];
    }
    return kSha256RoundConstants[round_constant_index];
  };

  for (uint32_t trial = 0; trial < trial_count; ++trial) {
    const uint32_t k = resolveK(trial);
    (void)word_dist(rng); // burn one word so the input stream offset matches CPU eval
    std::array<uint32_t, kStateWordCount> state{};
    for (uint32_t i = 0; i < kStateWordCount; ++i) {
      state[i] = word_dist(rng);
      if (kInputAOffset + i < variable_count) {
        plan.inputs[trial * variable_count + kInputAOffset + i] = static_cast<int32_t>(state[i]);
      }
    }
    const uint32_t w = word_dist(rng);
    if (kInputK < variable_count) {
      plan.inputs[trial * variable_count + kInputK] = static_cast<int32_t>(k);
    }
    if (kInputW < variable_count) {
      plan.inputs[trial * variable_count + kInputW] = static_cast<int32_t>(w);
    }
    if (kInputTrialId < variable_count) {
      plan.inputs[trial * variable_count + kInputTrialId] = static_cast<int32_t>(trial);
    }
    if (kInputRoundsCount < variable_count) {
      plan.inputs[trial * variable_count + kInputRoundsCount] =
          static_cast<int32_t>(rounds_per_trial);
    }

    // Reference: apply N rounds with the same K, W.
    std::array<uint32_t, kStateWordCount> expected = state;
    for (uint32_t r = 0; r < rounds_per_trial; ++r) {
      std::array<uint32_t, kStateWordCount> next{};
      referenceRound(expected[0], expected[1], expected[2], expected[3], expected[4],
                     expected[5], expected[6], expected[7], k, w, next.data());
      expected = next;
    }
    for (uint32_t i = 0; i < kStateWordCount; ++i) {
      plan.expected_outputs[trial * kStateWordCount + i] = expected[i];
    }
  }

  return plan;
}

} // anonymous namespace

// ====================================================================================
// Impl: holds CUDA handles + persistent device buffers.
// ====================================================================================
struct CudaSha256RoundEvaluator::Impl {
  // Configuration captured at construction.
  uint32_t variable_count;
  uint32_t trial_count;
  uint32_t round_constant_index;
  Sha256RoundEvaluator::RoundConstantsMode mode;
  uint32_t rounds_per_trial;
  uint32_t max_steps_per_trial;
  uint32_t effective_step_budget;

  // I/O bitmasks for the I/O-reflection opcodes. Sha256RoundEvaluator's variable
  // layout is fixed; we precompute these once and keep the device-side mirror.
  uint64_t is_input_mask = 0;
  uint64_t is_output_mask = 0;

  // Trial plan (constant for the lifetime of this evaluator).
  TrialPlan plan;

  // Persistent device buffers (sized to the largest call so far, reused thereafter).
  Instruction* d_instructions = nullptr;
  size_t d_instructions_capacity = 0;
  uint32_t* d_genome_offsets = nullptr;
  uint32_t* d_genome_lengths = nullptr;
  size_t d_offsets_capacity = 0;
  // Byte_to_insn lookup, packed per-genome (mirrors instructions SoA layout).
  uint32_t* d_byte_to_insn = nullptr;
  size_t d_byte_to_insn_capacity = 0;
  uint32_t* d_genome_byte_offsets = nullptr;
  uint32_t* d_genome_byte_lengths = nullptr;
  size_t d_byte_offsets_capacity = 0;
  // Per-genome CallSubroutine arg pool.
  CompiledCallArg* d_call_args = nullptr;
  size_t d_call_args_capacity = 0;
  uint32_t* d_call_arg_offsets = nullptr;
  size_t d_call_arg_offsets_capacity = 0;
  int32_t* d_inputs = nullptr;
  uint32_t* d_expected = nullptr;
  float* d_scores = nullptr;
  size_t d_scores_capacity = 0;

  // Subroutine library buffers. Filled by `setSubroutineLibrary()`; freed on
  // dtor. When the library is empty / not mounted, these stay null and `lib_count`
  // is 0; the kernel's CallSubroutine handler then treats the call as panic-
  // equivalent (matching the CPU evaluator).
  Instruction* d_lib_instructions = nullptr;
  uint32_t* d_lib_offsets = nullptr;
  uint32_t* d_lib_lengths = nullptr;
  uint32_t* d_lib_byte_to_insn = nullptr;
  uint32_t* d_lib_byte_offsets = nullptr;
  uint32_t* d_lib_byte_lengths = nullptr;
  uint8_t* d_lib_input_arities = nullptr;
  uint8_t* d_lib_output_arities = nullptr;
  uint32_t* d_lib_step_budgets = nullptr;
  uint32_t lib_count = 0;

  // Seed mixed into the per-trial curand init. Fixed by default so the GPU path
  // is deterministic across runs (matches the CPU evaluator's mt19937(0xA5A5C0DE)).
  uint64_t random_seed = 0xA5A5C0DEU;

  ~Impl() {
    if (d_instructions != nullptr) { cudaFree(d_instructions); }
    if (d_genome_offsets != nullptr) { cudaFree(d_genome_offsets); }
    if (d_genome_lengths != nullptr) { cudaFree(d_genome_lengths); }
    if (d_byte_to_insn != nullptr) { cudaFree(d_byte_to_insn); }
    if (d_genome_byte_offsets != nullptr) { cudaFree(d_genome_byte_offsets); }
    if (d_genome_byte_lengths != nullptr) { cudaFree(d_genome_byte_lengths); }
    if (d_call_args != nullptr) { cudaFree(d_call_args); }
    if (d_call_arg_offsets != nullptr) { cudaFree(d_call_arg_offsets); }
    if (d_inputs != nullptr) { cudaFree(d_inputs); }
    if (d_expected != nullptr) { cudaFree(d_expected); }
    if (d_scores != nullptr) { cudaFree(d_scores); }
    if (d_lib_instructions != nullptr) { cudaFree(d_lib_instructions); }
    if (d_lib_offsets != nullptr) { cudaFree(d_lib_offsets); }
    if (d_lib_lengths != nullptr) { cudaFree(d_lib_lengths); }
    if (d_lib_byte_to_insn != nullptr) { cudaFree(d_lib_byte_to_insn); }
    if (d_lib_byte_offsets != nullptr) { cudaFree(d_lib_byte_offsets); }
    if (d_lib_byte_lengths != nullptr) { cudaFree(d_lib_byte_lengths); }
    if (d_lib_input_arities != nullptr) { cudaFree(d_lib_input_arities); }
    if (d_lib_output_arities != nullptr) { cudaFree(d_lib_output_arities); }
    if (d_lib_step_budgets != nullptr) { cudaFree(d_lib_step_budgets); }
  }
};

CudaSha256RoundEvaluator::CudaSha256RoundEvaluator(const Sha256RoundEvaluator& config,
                                                   uint32_t variable_count)
    : impl_(std::make_unique<Impl>()) {
  impl_->variable_count = variable_count;
  impl_->trial_count = config.getTrialCount();
  impl_->round_constant_index = config.getRoundConstantIndex();
  impl_->mode = config.getRoundConstantsMode();
  impl_->rounds_per_trial = config.getRoundsPerTrial();
  impl_->max_steps_per_trial = config.getMaxStepsPerTrial();
  // Same effective step budget as the CPU evaluator: per-round budget * rounds.
  const uint64_t budget = static_cast<uint64_t>(impl_->max_steps_per_trial) *
                          static_cast<uint64_t>(impl_->rounds_per_trial);
  impl_->effective_step_budget = static_cast<uint32_t>(
      budget > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : budget);

  // Precompute the I/O bitmasks for the I/O-reflection opcodes. Sha256RoundEvaluator
  // pins variable layout: inputs are 0..7 (a..h), 8 (K), 9 (W), 10 (trial_id), 19
  // (rounds_count); outputs are 11..18. Match `Sha256RoundEvaluator::evaluate`.
  for (uint32_t i = 0; i < kStateWordCount; ++i) {
    impl_->is_input_mask |= 1ULL << (kInputAOffset + i);
    impl_->is_output_mask |= 1ULL << (kOutputAOffset + i);
  }
  impl_->is_input_mask |= 1ULL << kInputK;
  impl_->is_input_mask |= 1ULL << kInputW;
  impl_->is_input_mask |= 1ULL << kInputTrialId;
  impl_->is_input_mask |= 1ULL << kInputRoundsCount;

  // Build the trial plan up front -- same inputs / expected outputs are reused for
  // every `evaluate()` call.
  impl_->plan = buildTrialPlan(impl_->trial_count, impl_->variable_count,
                               impl_->round_constant_index, impl_->mode,
                               impl_->rounds_per_trial);

  // Upload the per-trial plan to its persistent device buffers once.
  const size_t inputs_bytes = impl_->plan.inputs.size() * sizeof(int32_t);
  const size_t expected_bytes = impl_->plan.expected_outputs.size() * sizeof(uint32_t);
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_inputs, inputs_bytes));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_expected, expected_bytes));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_inputs, impl_->plan.inputs.data(), inputs_bytes,
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_expected, impl_->plan.expected_outputs.data(),
                              expected_bytes, cudaMemcpyHostToDevice));
}

void CudaSha256RoundEvaluator::setSubroutineLibrary(
    const std::shared_ptr<beast::SubroutineLibrary>& library) {
  // Free any previously uploaded library first.
  if (impl_->d_lib_instructions != nullptr) {
    cudaFree(impl_->d_lib_instructions);
    impl_->d_lib_instructions = nullptr;
  }
  if (impl_->d_lib_offsets != nullptr) { cudaFree(impl_->d_lib_offsets); impl_->d_lib_offsets = nullptr; }
  if (impl_->d_lib_lengths != nullptr) { cudaFree(impl_->d_lib_lengths); impl_->d_lib_lengths = nullptr; }
  if (impl_->d_lib_byte_to_insn != nullptr) { cudaFree(impl_->d_lib_byte_to_insn); impl_->d_lib_byte_to_insn = nullptr; }
  if (impl_->d_lib_byte_offsets != nullptr) { cudaFree(impl_->d_lib_byte_offsets); impl_->d_lib_byte_offsets = nullptr; }
  if (impl_->d_lib_byte_lengths != nullptr) { cudaFree(impl_->d_lib_byte_lengths); impl_->d_lib_byte_lengths = nullptr; }
  if (impl_->d_lib_input_arities != nullptr) { cudaFree(impl_->d_lib_input_arities); impl_->d_lib_input_arities = nullptr; }
  if (impl_->d_lib_output_arities != nullptr) { cudaFree(impl_->d_lib_output_arities); impl_->d_lib_output_arities = nullptr; }
  if (impl_->d_lib_step_budgets != nullptr) { cudaFree(impl_->d_lib_step_budgets); impl_->d_lib_step_budgets = nullptr; }
  impl_->lib_count = 0;

  if (!library || library->empty()) {
    return;
  }

  // Compile every library entry. Each entry becomes a CompiledProgram; we flatten
  // their instruction streams and byte_to_insn tables into giant SoA arrays the
  // kernel can index by subroutine_id + offset.
  const uint32_t n = static_cast<uint32_t>(library->size());
  std::vector<Instruction> insns;
  std::vector<uint32_t> insn_offsets(n);
  std::vector<uint32_t> insn_lengths(n);
  std::vector<uint32_t> b2i;
  std::vector<uint32_t> b2i_offsets(n);
  std::vector<uint32_t> b2i_lengths(n);
  std::vector<uint8_t> in_arities(n);
  std::vector<uint8_t> out_arities(n);
  std::vector<uint32_t> step_budgets(n);
  for (uint32_t i = 0; i < n; ++i) {
    const auto& entry = (*library)[i];
    // SubroutineEntry::bytecode is std::vector<unsigned char>; compileProgramForGpu
    // expects std::vector<uint8_t>. Same on every platform we target, but the
    // explicit copy avoids relying on that assumption.
    const std::vector<uint8_t> bytes(entry.bytecode.begin(), entry.bytecode.end());
    auto cp = compileProgramForGpu(bytes, impl_->variable_count);
    insn_offsets[i] = static_cast<uint32_t>(insns.size());
    insn_lengths[i] = static_cast<uint32_t>(cp.instructions.size());
    insns.insert(insns.end(), cp.instructions.begin(), cp.instructions.end());
    b2i_offsets[i] = static_cast<uint32_t>(b2i.size());
    b2i_lengths[i] = static_cast<uint32_t>(cp.byte_to_insn.size());
    b2i.insert(b2i.end(), cp.byte_to_insn.begin(), cp.byte_to_insn.end());
    in_arities[i] = entry.input_arity;
    out_arities[i] = entry.output_arity;
    step_budgets[i] = entry.max_steps_per_call == 0 ? 1U : entry.max_steps_per_call;
  }
  impl_->lib_count = n;

  // Allocate + upload. Each cudaMalloc is a separate device allocation -- the data
  // is read-only on the device, but keeping it as plain arrays beats const-memory
  // for libraries that exceed 64 KB (the const-memory ceiling).
  const size_t insns_bytes = insns.size() * sizeof(Instruction);
  if (insns_bytes > 0) {
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_instructions, insns_bytes));
    BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_instructions, insns.data(), insns_bytes,
                                cudaMemcpyHostToDevice));
  }
  const size_t b2i_bytes = b2i.size() * sizeof(uint32_t);
  if (b2i_bytes > 0) {
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_byte_to_insn, b2i_bytes));
    BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_byte_to_insn, b2i.data(), b2i_bytes,
                                cudaMemcpyHostToDevice));
  }
  // Offsets/lengths arrays. Always non-empty for `lib_count > 0`.
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_offsets, n * sizeof(uint32_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_offsets, insn_offsets.data(),
                              n * sizeof(uint32_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_lengths, n * sizeof(uint32_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_lengths, insn_lengths.data(),
                              n * sizeof(uint32_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_byte_offsets, n * sizeof(uint32_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_byte_offsets, b2i_offsets.data(),
                              n * sizeof(uint32_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_byte_lengths, n * sizeof(uint32_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_byte_lengths, b2i_lengths.data(),
                              n * sizeof(uint32_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_input_arities, n * sizeof(uint8_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_input_arities, in_arities.data(),
                              n * sizeof(uint8_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_output_arities, n * sizeof(uint8_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_output_arities, out_arities.data(),
                              n * sizeof(uint8_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_lib_step_budgets, n * sizeof(uint32_t)));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_lib_step_budgets, step_budgets.data(),
                              n * sizeof(uint32_t), cudaMemcpyHostToDevice));
}

CudaSha256RoundEvaluator::~CudaSha256RoundEvaluator() = default;

std::vector<double>
CudaSha256RoundEvaluator::evaluate(const std::vector<std::vector<uint8_t>>& genomes,
                                   const std::atomic<bool>* stop_token) {
  const uint32_t num_genomes = static_cast<uint32_t>(genomes.size());
  std::vector<double> result(num_genomes, 0.0);
  if (num_genomes == 0) {
    return result;
  }

  // Honour stop-token as a fast-path return BEFORE we pay for any device work. The
  // kernel itself isn't interruptible mid-launch on this GPU generation; if the stop
  // flag flips during the launch we still wait for the kernel to finish and then
  // discard the scores. That's typically <100 ms for the configurations BEAST ships,
  // which is comparable to one CPU evaluator step.
  if (stop_token != nullptr && stop_token->load(std::memory_order_acquire)) {
    return result;
  }

  // Compile genomes on host. Cheap optimisation: a population frequently contains
  // duplicates (elitism preserves the best genome, and tournament selection
  // occasionally re-selects the same parent). We hash each genome's bytes and reuse
  // the compiled instruction stream when we've already seen it this batch -- saves
  // re-running ProgramParser + the jump-resolution pass for every duplicate.
  std::vector<CompiledProgram> compiled;
  compiled.reserve(num_genomes);
  std::vector<uint32_t> offsets(num_genomes, 0);
  std::vector<uint32_t> lengths(num_genomes, 0);
  std::vector<uint32_t> byte_offsets(num_genomes, 0);
  std::vector<uint32_t> byte_lengths(num_genomes, 0);
  std::vector<uint32_t> call_arg_offsets(num_genomes, 0);
  std::vector<Instruction> all_instructions;
  std::vector<uint32_t> all_byte_to_insn;
  std::vector<CompiledCallArg> all_call_args;
  // Reserve a guess; we'll grow if needed. 64 instructions per genome is the median
  // for the 1 KB-cap genomes the GA tends to produce.
  all_instructions.reserve(static_cast<size_t>(num_genomes) * 64);
  for (uint32_t g = 0; g < num_genomes; ++g) {
    CompiledProgram cp = compileProgramForGpu(genomes[g], impl_->variable_count);
    offsets[g] = static_cast<uint32_t>(all_instructions.size());
    lengths[g] = static_cast<uint32_t>(cp.instructions.size());
    all_instructions.insert(all_instructions.end(), cp.instructions.begin(),
                            cp.instructions.end());
    byte_offsets[g] = static_cast<uint32_t>(all_byte_to_insn.size());
    byte_lengths[g] = static_cast<uint32_t>(cp.byte_to_insn.size());
    all_byte_to_insn.insert(all_byte_to_insn.end(), cp.byte_to_insn.begin(),
                            cp.byte_to_insn.end());
    call_arg_offsets[g] = static_cast<uint32_t>(all_call_args.size());
    all_call_args.insert(all_call_args.end(), cp.call_subroutine_args.begin(),
                         cp.call_subroutine_args.end());
    compiled.push_back(std::move(cp));
  }

  // Even all-zero-length genomes are processed (the kernel just produces score 0 for
  // every trial). Guard against the zero-instruction case so cudaMalloc(0) doesn't
  // get called.
  if (all_instructions.empty()) {
    // Every genome is empty -> all-zero outputs -> score is `scoreWord(expected, 0)`
    // for each trial. Compute on host to avoid an empty kernel launch.
    for (uint32_t g = 0; g < num_genomes; ++g) {
      double sum = 0.0;
      for (uint32_t t = 0; t < impl_->trial_count; ++t) {
        double trial = 0.0;
        for (uint32_t i = 0; i < kStateWordCount; ++i) {
          const uint32_t expected = impl_->plan.expected_outputs[t * kStateWordCount + i];
          const uint32_t bits_wrong = popcount32(expected ^ 0U);
          trial += 1.0 - static_cast<double>(bits_wrong) /
                            static_cast<double>(kStateBitsPerWord);
        }
        sum += trial / static_cast<double>(kStateWordCount);
      }
      result[g] = sum / static_cast<double>(impl_->trial_count);
    }
    return result;
  }

  // Grow device buffers as needed.
  const size_t instructions_bytes = all_instructions.size() * sizeof(Instruction);
  if (instructions_bytes > impl_->d_instructions_capacity) {
    if (impl_->d_instructions != nullptr) {
      cudaFree(impl_->d_instructions);
    }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_instructions, instructions_bytes));
    impl_->d_instructions_capacity = instructions_bytes;
  }
  const size_t offsets_bytes = num_genomes * sizeof(uint32_t);
  if (offsets_bytes > impl_->d_offsets_capacity) {
    if (impl_->d_genome_offsets != nullptr) {
      cudaFree(impl_->d_genome_offsets);
    }
    if (impl_->d_genome_lengths != nullptr) {
      cudaFree(impl_->d_genome_lengths);
    }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_genome_offsets, offsets_bytes));
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_genome_lengths, offsets_bytes));
    impl_->d_offsets_capacity = offsets_bytes;
  }

  // Byte-to-instruction table buffers (used by variable-target jumps). One per
  // genome, concatenated. Empty when no genome uses variable-target jumps; we
  // still keep a valid (but small) device pointer so the kernel doesn't see
  // nullptr.
  const size_t b2i_bytes = all_byte_to_insn.size() * sizeof(uint32_t);
  if (b2i_bytes > impl_->d_byte_to_insn_capacity) {
    if (impl_->d_byte_to_insn != nullptr) { cudaFree(impl_->d_byte_to_insn); }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_byte_to_insn, std::max<size_t>(b2i_bytes, 4U)));
    impl_->d_byte_to_insn_capacity = std::max<size_t>(b2i_bytes, 4U);
  }
  if (offsets_bytes > impl_->d_byte_offsets_capacity) {
    if (impl_->d_genome_byte_offsets != nullptr) { cudaFree(impl_->d_genome_byte_offsets); }
    if (impl_->d_genome_byte_lengths != nullptr) { cudaFree(impl_->d_genome_byte_lengths); }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_genome_byte_offsets, offsets_bytes));
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_genome_byte_lengths, offsets_bytes));
    impl_->d_byte_offsets_capacity = offsets_bytes;
  }

  // CallSubroutine arg pool buffers. Always allocate at least 1 entry so the
  // kernel pointer is non-null (the kernel never dereferences it unless
  // CallSubroutine fires, but defensive).
  const size_t call_args_bytes =
      std::max<size_t>(all_call_args.size() * sizeof(CompiledCallArg), sizeof(CompiledCallArg));
  if (call_args_bytes > impl_->d_call_args_capacity) {
    if (impl_->d_call_args != nullptr) { cudaFree(impl_->d_call_args); }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_call_args, call_args_bytes));
    impl_->d_call_args_capacity = call_args_bytes;
  }
  if (offsets_bytes > impl_->d_call_arg_offsets_capacity) {
    if (impl_->d_call_arg_offsets != nullptr) { cudaFree(impl_->d_call_arg_offsets); }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_call_arg_offsets, offsets_bytes));
    impl_->d_call_arg_offsets_capacity = offsets_bytes;
  }

  // The kernel emits ONE accumulated score per genome (trials averaged inside the
  // kernel, since the per-thread sequential trial loop already has the sum in
  // registers).
  const size_t scores_bytes = static_cast<size_t>(num_genomes) * sizeof(float);
  if (scores_bytes > impl_->d_scores_capacity) {
    if (impl_->d_scores != nullptr) {
      cudaFree(impl_->d_scores);
    }
    BEAST_CUDA_CHECK(cudaMalloc(&impl_->d_scores, scores_bytes));
    impl_->d_scores_capacity = scores_bytes;
  }

  // Upload compiled programs and offsets.
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_instructions, all_instructions.data(),
                              instructions_bytes, cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_offsets, offsets.data(), offsets_bytes,
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_lengths, lengths.data(), offsets_bytes,
                              cudaMemcpyHostToDevice));
  if (b2i_bytes > 0) {
    BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_byte_to_insn, all_byte_to_insn.data(),
                                b2i_bytes, cudaMemcpyHostToDevice));
  }
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_byte_offsets, byte_offsets.data(),
                              offsets_bytes, cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_byte_lengths, byte_lengths.data(),
                              offsets_bytes, cudaMemcpyHostToDevice));
  if (!all_call_args.empty()) {
    BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_call_args, all_call_args.data(),
                                all_call_args.size() * sizeof(CompiledCallArg),
                                cudaMemcpyHostToDevice));
  }
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_call_arg_offsets, call_arg_offsets.data(),
                              offsets_bytes, cudaMemcpyHostToDevice));

  // Launch geometry: one thread per genome. For typical population sizes (32-128)
  // this is a small grid relative to what Ada can run, but parity with the CPU
  // evaluator's sequential-trial semantics requires this thread layout. Block size
  // of 64 keeps the launch reasonable even for population=16 (one block, 16 active
  // threads); for bigger populations we round up to whole blocks naturally.
  const uint32_t block_size = 64;
  const uint32_t grid_size = (num_genomes + block_size - 1) / block_size;
  sha256RoundKernel<<<grid_size, block_size>>>(
      impl_->d_instructions, impl_->d_genome_offsets, impl_->d_genome_lengths,
      impl_->d_byte_to_insn, impl_->d_genome_byte_offsets, impl_->d_genome_byte_lengths,
      impl_->d_call_args, impl_->d_call_arg_offsets,
      num_genomes,
      impl_->is_input_mask, impl_->is_output_mask,
      impl_->d_lib_instructions, impl_->d_lib_offsets, impl_->d_lib_lengths,
      impl_->d_lib_byte_to_insn, impl_->d_lib_byte_offsets, impl_->d_lib_byte_lengths,
      impl_->d_lib_input_arities, impl_->d_lib_output_arities, impl_->d_lib_step_budgets,
      impl_->lib_count,
      impl_->random_seed,
      impl_->d_inputs, impl_->d_expected, impl_->variable_count, impl_->trial_count,
      impl_->effective_step_budget, impl_->d_scores);
  BEAST_CUDA_CHECK(cudaGetLastError());

  // Pull per-genome scores back; the kernel has already done the trial-count average.
  std::vector<float> host_scores(num_genomes, 0.0F);
  BEAST_CUDA_CHECK(cudaMemcpy(host_scores.data(), impl_->d_scores, scores_bytes,
                              cudaMemcpyDeviceToHost));

  for (uint32_t g = 0; g < num_genomes; ++g) {
    result[g] = static_cast<double>(host_scores[g]);
  }
  return result;
}

double CudaSha256RoundEvaluator::evaluateOne(const std::vector<uint8_t>& genome) {
  auto scores = evaluate({genome}, nullptr);
  return scores.empty() ? 0.0 : scores.front();
}

bool isCudaAvailable() noexcept {
  // Cache the result. A failed first call (no driver, no device, ...) shouldn't make
  // every subsequent call pay the same cudaGetDeviceCount round-trip.
  static std::once_flag flag;
  static bool available = false;
  std::call_once(flag, [] {
    int count = 0;
    const cudaError_t err = cudaGetDeviceCount(&count);
    available = err == cudaSuccess && count > 0;
    if (!available) {
      (void)cudaGetLastError(); // clear any sticky error so unrelated code doesn't trip
    }
  });
  return available;
}

} // namespace beast::cuda
