#ifndef BEAST_CUDA_INSTRUCTION_HPP_
#define BEAST_CUDA_INSTRUCTION_HPP_

// Standard
#include <cstdint>

namespace beast::cuda {

/**
 * @brief GPU-side opcode set -- a deliberately tight subset of BEAST `OpCode`.
 *
 * Only the operations the device VM actually executes appear here. Everything else is
 * folded to `NoOp` at compile time on the host (`compileProgramForGpu`) so the device
 * VM's switch dispatch stays small, branch-predictable, and free of host-only
 * functionality (string tables, printing, system calls, dynamic variable declaration,
 * stacks, subroutines).
 *
 * The numeric values are stable on-wire between the host compiler and the device VM
 * but are NOT the same as `beast::OpCode`; the compiler is the translation seam. New
 * device opcodes go at the END of the enum, never inserted -- there are device
 * binaries cached in unit-test fixtures that depend on these values.
 *
 * Why this enum is plain `uint8_t` rather than `enum class`:
 *   - We pack it into a 16-byte `Instruction` struct that needs to be `__device__`
 *     copyable, and CUDA pre-12 had quirks with strongly-typed enums inside POD
 *     uploaded via `cudaMemcpy`. Defensive choice with negligible API cost.
 */
enum DeviceOpcode : uint8_t {
  // Folded / no-op family ------------------------------------------------------------
  Op_NoOp = 0,             ///< Do nothing. Used as the host-compile fallback for any
                           ///< BEAST opcode the device VM does not implement.
  Op_Terminate = 1,        ///< Halt the VM at the current PC; trial proceeds to scoring.

  // Variable management --------------------------------------------------------------
  Op_SetVarConst = 2,      ///< vars[op0] = op1
  Op_CopyVarToVar = 3,     ///< vars[op1] = vars[op0]
  Op_SwapVars = 4,         ///< std::swap(vars[op0], vars[op1])

  // Arithmetic -----------------------------------------------------------------------
  Op_AddConstToVar = 5,            ///< vars[op0] += op1
  Op_AddVarToVar = 6,              ///< vars[op1] += vars[op0]
  Op_SubtractConstFromVar = 7,     ///< vars[op0] -= op1
  Op_SubtractVarFromVar = 8,       ///< vars[op1] -= vars[op0]
  Op_ModuloConst = 9,              ///< vars[op0] %= op1 (skip when op1 == 0)
  Op_ModuloVar = 10,               ///< vars[op1] %= vars[op0] (skip when 0)

  // Min / max ------------------------------------------------------------------------
  Op_GetMaxConst = 11,             ///< vars[op0] = max(vars[op0], op1)
  Op_GetMinConst = 12,             ///< vars[op0] = min(vars[op0], op1)
  Op_GetMaxVar = 13,               ///< vars[op1] = max(vars[op1], vars[op0])
  Op_GetMinVar = 14,               ///< vars[op1] = min(vars[op1], vars[op0])

  // Comparison (result lands in op0 as 0 or 1) ---------------------------------------
  Op_CmpGtConst = 15,              ///< vars[op0] = (vars[op0] > op1) ? 1 : 0
  Op_CmpLtConst = 16,
  Op_CmpEqConst = 17,
  Op_CmpGtVar = 18,                ///< vars[op1] = (vars[op0] > vars[op1]) ? 1 : 0
  Op_CmpLtVar = 19,
  Op_CmpEqVar = 20,

  // Bit ops --------------------------------------------------------------------------
  Op_BitShiftLeftConst = 21,       ///< vars[op0] <<= op1 (mask amount to 0..31)
  Op_BitShiftRightConst = 22,
  Op_VariableBitShiftLeft = 23,    ///< vars[op0] <<= vars[op1] (mask 0..31)
  Op_VariableBitShiftRight = 24,
  Op_RotateLeftConst = 25,         ///< 32-bit ROL of vars[op0] by op1
  Op_RotateRightConst = 26,        ///< 32-bit ROR of vars[op0] by op1
  Op_VariableRotateLeft = 27,
  Op_VariableRotateRight = 28,
  Op_BitwiseInvert = 29,           ///< vars[op0] = ~vars[op0]
  Op_BitwiseAnd = 30,              ///< vars[op1] &= vars[op0]
  Op_BitwiseOr = 31,               ///< vars[op1] |= vars[op0]
  Op_BitwiseXor = 32,              ///< vars[op1] ^= vars[op0]

  // Jumps (constant immediate targets only; the host compiler resolves byte offsets
  // to instruction indices at compile time). Variable-target jumps are folded to NoOp.
  Op_RelJumpIfVarGt0 = 33,         ///< if (vars[op0] > 0) pc = op1 (insn index)
  Op_RelJumpIfVarLt0 = 34,
  Op_RelJumpIfVarEq0 = 35,
  Op_UnconditionalJump = 36,       ///< pc = op1

  // Reflection-ish helpers -----------------------------------------------------------
  Op_LoadMemorySize = 37,          ///< vars[op0] = total variable count
  Op_LoadCurrentAddress = 38,      ///< vars[op0] = current PC (insn index)

  // ----- Tier 3 additions (full opcode coverage) ------------------------------------

  // Declare/Undeclare. Maintained as a per-thread 64-bit `declared_mask`. The mask
  // starts with all I/O vars pre-declared by the host setup; the program can extend
  // it with `DeclareVariable` and shrink it with `UndeclareVariable`. Access to an
  // undeclared var is skipped silently in the kernel (matches the CPU evaluator's
  // outer try/catch).
  Op_DeclareVar = 39,              ///< declared_mask |= (1 << op0)
  Op_UndeclareVar = 40,            ///< declared_mask &= ~(1 << op0)

  // I/O reflection. Inputs and outputs are pinned at the host setup (var ranges are
  // known at compile time); these opcodes resolve to constant lookups against the
  // host-supplied input/output bitmaps stored in the compiled program.
  Op_CheckIfVarIsInput = 41,       ///< vars[op0] = is_input_mask & (1 << op0) ? 1 : 0
  Op_CheckIfVarIsOutput = 42,      ///< vars[op0] = is_output_mask & (1 << op0) ? 1 : 0
  Op_LoadInputCount = 43,          ///< vars[op0] = popcount(is_input_mask)
  Op_LoadOutputCount = 44,         ///< vars[op0] = popcount(is_output_mask)
  Op_CheckIfInputWasSet = 45,      ///< vars[op0] = input_changed_mask & (1 << op0) ? 1 : 0

  // String table state. Per-thread storage lives in shared memory: `string_count`
  // entries of `string_item_len` bytes. The 12 string-table opcodes map to direct
  // reads/writes on this buffer; PrintVariableStringFromStringTable / its constant
  // sibling are no-ops on the device (scoring never reads the print buffer).
  Op_LoadStringTableLimit = 46,    ///< vars[op0] = string_count
  Op_LoadStringTableItemLen = 47,  ///< vars[op0] = string_item_len
  Op_SetStringTableEntry = 48,     ///< strings[op0][0..string_item_len) = aux-referenced bytes
  Op_SetVarStringTableEntry = 49,  ///< strings[vars[op0]][0..N) = aux-referenced bytes
  Op_LoadStringItemLen = 50,       ///< vars[op0] = strlen(strings[op1])
  Op_LoadVarStringItemLen = 51,    ///< vars[op0] = strlen(strings[vars[op1]])
  Op_LoadStringItemIntoVars = 52,  ///< vars[op0..op0+N) = strings[op1] (one char per var)
  Op_LoadVarStringItemIntoVars = 53, ///< vars[op0..op0+N) = strings[vars[op1]]

  // Random. Per-thread `curand_state` seeded deterministically at kernel entry. Adds
  // ~48 bytes of per-thread state -- only paid by genomes that use the opcode (the
  // compiler emits Op_NoOp if the program doesn't reference random).
  Op_LoadRandomValue = 54,         ///< vars[op0] = curand(state)

  // Variable-target jumps. The byte target is resolved at runtime against the
  // byte-to-instruction-index table (`prog.byte_to_insn`) uploaded alongside the
  // program. Falls through to next insn (== folded NoOp) when the target byte
  // doesn't land on a known instruction boundary. Adds warp divergence proportional
  // to the program's branchiness.
  //
  // Wire format: op0 = condition variable (or unused for unconditional),
  //              op1 = address variable (vars[op1] is the byte target),
  //              aux = encoded predicate kind (see kernel switch).
  Op_VarTargetJump = 55,           ///< unified family, dispatched by aux

  // Subroutines. Per-thread call frame in shared memory; bounded depth (kMaxCallDepth
  // in the kernel). Wire format: op0 = subroutine_id, op1 = input_count,
  //                              op2 = output_count, aux = input/output var indices
  //                              packed into a side table (per-program).
  // Implementation detail: input/output args live in the per-program "callsub side
  // table" rather than the 12-byte operand area because the arity can be up to
  // kMaxSubroutineArity (4) and each arg is a (var_index, follow_flag) pair.
  Op_CallSubroutine = 56,          ///< callee runs subroutine_id with caller-side args

  // Stack operations. BEAST stacks aren't separate objects; the "stack" variable
  // index holds the stack pointer, and items live in subsequent variables. These
  // 5 opcodes are functionally compositions of existing variable read/write ops,
  // but it's cleaner to give them proper opcode slots than hide them inside another
  // dispatch.
  Op_PushVarOnStack = 57,          ///< vars[op0] becomes size N, vars[op0+N] = vars[op1]
  Op_PushConstOnStack = 58,        ///< vars[op0] becomes size N, vars[op0+N] = op1
  Op_PopVarFromStack = 59,         ///< vars[op1] = vars[op0 + vars[op0]]; vars[op0] -= 1
  Op_PopTopFromStack = 60,         ///< vars[op0] -= 1 (and zero the slot)
  Op_CheckStackEmpty = 61,         ///< vars[op0] = (vars[op1] == 0) ? 1 : 0

  Op_Count                          ///< Sentinel; not a real opcode
};

/**
 * @brief One pre-decoded device-side instruction. 16 bytes, aligned.
 *
 * The host compiler walks the BEAST byte stream once, decodes each instruction into
 * this fixed-width form, resolves any constant-immediate jump targets to instruction
 * indices, and folds everything the device doesn't implement into `Op_NoOp`. The
 * device VM then dispatches on `opcode` with no per-step parser overhead -- this is
 * the single biggest win over running the CPU VM byte-stream interpreter on the GPU.
 *
 * Layout is deliberately:
 *   - `opcode` first so the cold dispatch path loads a single byte before deciding
 *     whether to bother with the operands.
 *   - Two padding bytes plus a 16-bit `aux` field. `aux` is reserved for future
 *     features (compile-time hints; per-instruction tracing flags); currently 0.
 *   - Three 32-bit operands. Most instructions use 0, 1, or 2; three is the maximum
 *     needed by any opcode in the current set. Unused operands stay 0.
 */
struct Instruction {
  uint8_t opcode = Op_NoOp;
  uint8_t pad0 = 0;
  uint16_t aux = 0;
  int32_t op0 = 0;
  int32_t op1 = 0;
  int32_t op2 = 0;
};

// Sanity-check the layout: keep instruction transfers and stride math simple. If this
// ever fires we have an ABI break and the corresponding device binaries should be
// rebuilt.
static_assert(sizeof(Instruction) == 16, "Instruction must be 16 bytes for GPU layout");

} // namespace beast::cuda

#endif // BEAST_CUDA_INSTRUCTION_HPP_
