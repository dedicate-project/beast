#include <beast/cuda/compiler.hpp>

// Standard
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

// Internal
#include <beast/opcodes.hpp>
#include <beast/program_parser.hpp>

namespace beast::cuda {

namespace {

// Convenience: read a little-endian int32_t from `bytes` starting at `offset`. The
// caller has already validated that the span exists, so we don't bounds-check here.
// Mirrors `VmSession::getData4`.
int32_t readInt32LE(const std::vector<uint8_t>& bytes, uint32_t offset) {
  return static_cast<int32_t>(
      static_cast<uint32_t>(bytes[offset]) |
      (static_cast<uint32_t>(bytes[offset + 1]) << 8U) |
      (static_cast<uint32_t>(bytes[offset + 2]) << 16U) |
      (static_cast<uint32_t>(bytes[offset + 3]) << 24U));
}

int16_t readInt16LE(const std::vector<uint8_t>& bytes, uint32_t offset) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[offset]) |
                              (static_cast<uint16_t>(bytes[offset + 1]) << 8U));
}

int8_t readInt8(const std::vector<uint8_t>& bytes, uint32_t offset) {
  return static_cast<int8_t>(bytes[offset]);
}

// Clamp `index` into [0, count). Out-of-range indices in the source program are
// remapped to 0 -- the same defensive policy the CPU VM applies for invalid variable
// references (it throws there; we don't have exception-throw support on the device, so
// we silently fold to a benign no-op-equivalent index).
uint32_t clampVarIndex(int32_t index, uint32_t variable_count) {
  if (variable_count == 0) {
    return 0;
  }
  if (index < 0) {
    return 0;
  }
  const auto unsigned_index = static_cast<uint32_t>(index);
  return unsigned_index >= variable_count ? 0 : unsigned_index;
}

// Encode the variable-target jump predicate in `Instruction::aux`. The kernel
// switches on this value to pick the right "evaluate the condition" path. Kept as
// a small enum (not bit flags) to avoid carrying bool combinations the wire format
// has to interpret -- pattern matching one byte is cheaper than checking three.
enum VarJumpKind : uint16_t {
  kVarJumpAlwaysAbs = 0,
  kVarJumpAlwaysRel = 1,
  kVarJumpIfGt0Abs = 2,
  kVarJumpIfLt0Abs = 3,
  kVarJumpIfEq0Abs = 4,
  kVarJumpIfGt0Rel = 5,
  kVarJumpIfLt0Rel = 6,
  kVarJumpIfEq0Rel = 7,
};

} // namespace

CompiledProgram compileProgramForGpu(const std::vector<uint8_t>& program_data,
                                     uint32_t variable_count) {
  CompiledProgram out;
  if (program_data.empty() || variable_count == 0) {
    return out;
  }

  // ProgramParser wants `std::vector<unsigned char>`; on every platform we target that
  // is layout-compatible with `std::vector<uint8_t>` and the copy is cheap relative to
  // the work we're about to do.
  const std::vector<unsigned char> parser_bytes(program_data.begin(), program_data.end());
  const auto parse_result = ProgramParser::parse(parser_bytes);
  if (parse_result.spans.empty()) {
    return out;
  }

  // Pass 1: decode each span into a `cuda::Instruction`. For jump opcodes we
  // temporarily stash the BYTE offset (relative or absolute, depending on the source
  // opcode) in `op1`; pass 2 rewrites those to instruction indices.
  out.instructions.reserve(parse_result.spans.size());

  // Map source byte offset -> instruction index, used by the jump back-patch pass to
  // resolve constant-target jumps (and at runtime by the device VM for variable-target
  // jumps). Byte offsets are bounded by program size so a dense vector is faster than
  // a hash map (large NoOp-padded programs would otherwise burn most of the compile
  // time on hash insertions, which dominated the first profiling run by ~200ms per
  // generation).
  std::vector<uint32_t>& byte_to_insn = out.byte_to_insn;
  byte_to_insn.assign(program_data.size(), kNoInstructionMapping);

  // Track each span's source byte length so pass 2 can compute "next instruction byte
  // offset" for relative jumps.
  std::vector<uint32_t> span_byte_lengths;
  span_byte_lengths.reserve(parse_result.spans.size());

  for (const auto& span : parse_result.spans) {
    const uint32_t insn_index = static_cast<uint32_t>(out.instructions.size());
    byte_to_insn[span.offset] = insn_index;
    span_byte_lengths.push_back(span.length);

    Instruction insn; // defaults to Op_NoOp
    const uint32_t body = span.offset + 1; // operands start after the 1-byte opcode

    switch (span.opcode) {
      // -------- Folded / control --------
      case OpCode::NoOp:
        insn.opcode = Op_NoOp;
        break;
      case OpCode::Terminate:
      case OpCode::TerminateWithVariableReturnCode:
        // We don't carry the return code -- nobody reads it on the device path.
        insn.opcode = Op_Terminate;
        break;

      // -------- Variable management --------
      case OpCode::SetVariable: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t value = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_SetVarConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = value;
        out.max_variable_index = std::max(out.max_variable_index, static_cast<uint32_t>(insn.op0));
        break;
      }
      case OpCode::CopyVariable: {
        const int32_t src = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CopyVarToVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(src, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        out.max_variable_index =
            std::max({out.max_variable_index, static_cast<uint32_t>(insn.op0),
                      static_cast<uint32_t>(insn.op1)});
        break;
      }
      case OpCode::SwapVariables: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_SwapVars;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }

      // -------- Arithmetic --------
      case OpCode::AddConstantToVariable: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_AddConstToVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::AddVariableToVariable: {
        const int32_t src = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_AddVarToVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(src, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        break;
      }
      case OpCode::SubtractConstantFromVariable: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_SubtractConstFromVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::SubtractVariableFromVariable: {
        const int32_t src = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_SubtractVarFromVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(src, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        break;
      }
      case OpCode::ModuloVariableByConstant: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_ModuloConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::ModuloVariableByVariable: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t mod_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_ModuloVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(mod_var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }

      // -------- Min / max --------
      case OpCode::GetMaxOfVariableAndConstant: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_GetMaxConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::GetMinOfVariableAndConstant: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_GetMinConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::GetMaxOfVariableAndVariable: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_GetMaxVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }
      case OpCode::GetMinOfVariableAndVariable: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_GetMinVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }

      // -------- Comparison --------
      // CPU semantics: result written back into the FIRST variable as 0 or 1.
      case OpCode::CompareIfVariableGtConstant: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CmpGtConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::CompareIfVariableLtConstant: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CmpLtConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::CompareIfVariableEqConstant: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CmpEqConst;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::CompareIfVariableGtVariable: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CmpGtVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }
      case OpCode::CompareIfVariableLtVariable: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CmpLtVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }
      case OpCode::CompareIfVariableEqVariable: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CmpEqVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }

      // -------- Bit ops --------
      case OpCode::BitShiftVariableLeft: {
        const int32_t var = readInt32LE(program_data, body);
        const int8_t places = readInt8(program_data, body + 4 + 1);
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        if (places >= 0) {
          insn.opcode = Op_BitShiftLeftConst;
          insn.op1 = places;
        } else {
          // CPU: negative count for "shift left" means actually shift right by -count.
          insn.opcode = Op_BitShiftRightConst;
          insn.op1 = -static_cast<int32_t>(places);
        }
        break;
      }
      case OpCode::BitShiftVariableRight: {
        const int32_t var = readInt32LE(program_data, body);
        const int8_t places = readInt8(program_data, body + 4 + 1);
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        if (places >= 0) {
          insn.opcode = Op_BitShiftRightConst;
          insn.op1 = places;
        } else {
          insn.opcode = Op_BitShiftLeftConst;
          insn.op1 = -static_cast<int32_t>(places);
        }
        break;
      }
      case OpCode::VariableBitShiftVariableLeft: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t places_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_VariableBitShiftLeft;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(places_var, variable_count));
        break;
      }
      case OpCode::VariableBitShiftVariableRight: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t places_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_VariableBitShiftRight;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(places_var, variable_count));
        break;
      }
      case OpCode::RotateVariableLeft: {
        const int32_t var = readInt32LE(program_data, body);
        const int8_t places = readInt8(program_data, body + 4 + 1);
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        if (places >= 0) {
          insn.opcode = Op_RotateLeftConst;
          insn.op1 = places;
        } else {
          insn.opcode = Op_RotateRightConst;
          insn.op1 = -static_cast<int32_t>(places);
        }
        break;
      }
      case OpCode::RotateVariableRight: {
        const int32_t var = readInt32LE(program_data, body);
        const int8_t places = readInt8(program_data, body + 4 + 1);
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        if (places >= 0) {
          insn.opcode = Op_RotateRightConst;
          insn.op1 = places;
        } else {
          insn.opcode = Op_RotateLeftConst;
          insn.op1 = -static_cast<int32_t>(places);
        }
        break;
      }
      case OpCode::VariableRotateVariableLeft: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t places_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_VariableRotateLeft;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(places_var, variable_count));
        break;
      }
      case OpCode::VariableRotateVariableRight: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t places_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_VariableRotateRight;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(places_var, variable_count));
        break;
      }
      case OpCode::BitWiseInvertVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_BitwiseInvert;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::BitWiseAndTwoVariables: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_BitwiseAnd;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }
      case OpCode::BitWiseOrTwoVariables: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_BitwiseOr;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }
      case OpCode::BitWiseXorTwoVariables: {
        const int32_t a = readInt32LE(program_data, body);
        const int32_t b = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_BitwiseXor;
        insn.op0 = static_cast<int32_t>(clampVarIndex(a, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(b, variable_count));
        break;
      }

      // -------- Jumps (constant target). Back-patched in pass 2. --------
      //
      // Stash:
      //   - jump_kind  in `aux`              (1 = relative, 2 = absolute)
      //   - byte_target in `op1`             (signed; absolute or relative)
      // Pass 2 resolves these to an instruction-index target. If resolution fails the
      // jump folds to Op_NoOp.
      case OpCode::RelativeJumpIfVariableGt0:
      case OpCode::RelativeJumpIfVariableLt0:
      case OpCode::RelativeJumpIfVariableEq0:
      case OpCode::AbsoluteJumpIfVariableGt0:
      case OpCode::AbsoluteJumpIfVariableLt0:
      case OpCode::AbsoluteJumpIfVariableEq0: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t addr = readInt32LE(program_data, body + 4 + 1);
        const bool is_relative = span.opcode == OpCode::RelativeJumpIfVariableGt0 ||
                                 span.opcode == OpCode::RelativeJumpIfVariableLt0 ||
                                 span.opcode == OpCode::RelativeJumpIfVariableEq0;
        const bool is_gt = span.opcode == OpCode::RelativeJumpIfVariableGt0 ||
                           span.opcode == OpCode::AbsoluteJumpIfVariableGt0;
        const bool is_lt = span.opcode == OpCode::RelativeJumpIfVariableLt0 ||
                           span.opcode == OpCode::AbsoluteJumpIfVariableLt0;
        insn.opcode = is_gt   ? Op_RelJumpIfVarGt0
                     : is_lt  ? Op_RelJumpIfVarLt0
                              : Op_RelJumpIfVarEq0;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        insn.op1 = addr;
        insn.aux = is_relative ? 1U : 2U;
        break;
      }
      case OpCode::UnconditionalJumpToRelativeAddress:
      case OpCode::UnconditionalJumpToAbsoluteAddress: {
        const int32_t addr = readInt32LE(program_data, body);
        const bool is_relative = span.opcode == OpCode::UnconditionalJumpToRelativeAddress;
        insn.opcode = Op_UnconditionalJump;
        insn.op1 = addr;
        insn.aux = is_relative ? 1U : 2U;
        break;
      }

      // -------- Reflection-ish helpers --------
      case OpCode::LoadMemorySizeIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadMemorySize;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::LoadCurrentAddressIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadCurrentAddress;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }

      // -------- Tier 3: variable declarations --------
      // The wire format for DeclareVariable is `(int32 var_index, int8 type)`. We
      // ignore the type byte on the device: BEAST has Int32 and Link types; the
      // device VM treats every var as Int32 (Link follow-ups would need unbounded
      // indirection chains, which is what Phase 5's CPU fallback exists for). Genomes
      // that actually declare Link vars are routed by the hybrid dispatcher.
      case OpCode::DeclareVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_DeclareVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::UndeclareVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_UndeclareVar;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }

      // -------- Tier 3: I/O reflection --------
      case OpCode::CheckIfVariableIsInput: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CheckIfVarIsInput;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::CheckIfVariableIsOutput: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CheckIfVarIsOutput;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::LoadInputCountIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadInputCount;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::LoadOutputCountIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadOutputCount;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::CheckIfInputWasSet: {
        const int32_t var = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CheckIfInputWasSet;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }

      // -------- Tier 3: string table reflection --------
      case OpCode::LoadStringTableLimitIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadStringTableLimit;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }
      case OpCode::LoadStringTableItemLengthLimitIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadStringTableItemLen;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        break;
      }

      // -------- Tier 3: string table writes --------
      // SetStringTableEntry wire format: `int32 entry_index, int16 string_length,
      // <bytes>`. We don't carry the string bytes through to the device -- the
      // string table on the device is a fixed-size shmem buffer and the per-thread
      // "what's in slot X" only needs the inline literal IF a downstream
      // LoadStringItem* reads it back. For now, we model the write as "clear slot X
      // to a single zero byte" which preserves the LENGTH side of the contract (the
      // CPU evaluator records the length too); programs that depend on the actual
      // string contents at byte-level go through the CPU fallback. This is enough
      // for the common case of "Hello\0" style sentinels in evaluator programs.
      case OpCode::SetStringTableEntry: {
        const int32_t slot = readInt32LE(program_data, body);
        const int16_t length = readInt16LE(program_data, body + 4);
        insn.opcode = Op_SetStringTableEntry;
        insn.op0 = slot;
        insn.op1 = length;
        break;
      }
      case OpCode::SetVariableStringTableEntry: {
        const int32_t slot_var = readInt32LE(program_data, body);
        const int16_t length = readInt16LE(program_data, body + 4 + 1);
        insn.opcode = Op_SetVarStringTableEntry;
        insn.op0 = static_cast<int32_t>(clampVarIndex(slot_var, variable_count));
        insn.op1 = length;
        break;
      }

      // -------- Tier 3: string table reads --------
      case OpCode::LoadStringItemLengthIntoVariable: {
        const int32_t slot = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4);
        insn.opcode = Op_LoadStringItemLen;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = slot;
        break;
      }
      case OpCode::LoadVariableStringItemLengthIntoVariable: {
        const int32_t slot_var = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_LoadVarStringItemLen;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(slot_var, variable_count));
        break;
      }
      case OpCode::LoadStringItemIntoVariables: {
        const int32_t slot = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4);
        insn.opcode = Op_LoadStringItemIntoVars;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = slot;
        break;
      }
      case OpCode::LoadVariableStringItemIntoVariables: {
        const int32_t slot_var = readInt32LE(program_data, body);
        const int32_t dst = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_LoadVarStringItemIntoVars;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(slot_var, variable_count));
        break;
      }

      // -------- Tier 3: random --------
      // Activates the per-thread curand_state path at kernel launch time. The state
      // is seeded deterministically per (genome, trial) so two runs of the same
      // program with the same seed produce the same scores -- matching the CPU
      // evaluator's mt19937 seeding policy.
      case OpCode::LoadRandomValueIntoVariable: {
        const int32_t var = readInt32LE(program_data, body);
        insn.opcode = Op_LoadRandomValue;
        insn.op0 = static_cast<int32_t>(clampVarIndex(var, variable_count));
        out.uses_random = true;
        break;
      }

      // -------- Tier 3: stack ops (compose to var ops at runtime) -----------------
      // BEAST stacks aren't separate data structures -- the stack pointer lives in
      // vars[base], and the items live in vars[base+1..]. The kernel implements
      // these as 2-4 variable read/writes; nothing new on the storage side.
      case OpCode::PushVariableOnStack: {
        const int32_t stack_var = readInt32LE(program_data, body);
        const int32_t value_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_PushVarOnStack;
        insn.op0 = static_cast<int32_t>(clampVarIndex(stack_var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(value_var, variable_count));
        break;
      }
      case OpCode::PushConstantOnStack: {
        const int32_t stack_var = readInt32LE(program_data, body);
        const int32_t constant = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_PushConstOnStack;
        insn.op0 = static_cast<int32_t>(clampVarIndex(stack_var, variable_count));
        insn.op1 = constant;
        break;
      }
      case OpCode::PopVariableFromStack: {
        const int32_t stack_var = readInt32LE(program_data, body);
        const int32_t dst_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_PopVarFromStack;
        insn.op0 = static_cast<int32_t>(clampVarIndex(stack_var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(dst_var, variable_count));
        break;
      }
      case OpCode::PopTopItemFromStack: {
        const int32_t stack_var = readInt32LE(program_data, body);
        insn.opcode = Op_PopTopFromStack;
        insn.op0 = static_cast<int32_t>(clampVarIndex(stack_var, variable_count));
        break;
      }
      case OpCode::CheckIfStackIsEmpty: {
        const int32_t stack_var = readInt32LE(program_data, body);
        const int32_t dst_var = readInt32LE(program_data, body + 4 + 1);
        insn.opcode = Op_CheckStackEmpty;
        insn.op0 = static_cast<int32_t>(clampVarIndex(dst_var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(stack_var, variable_count));
        break;
      }

      // -------- Tier 3: variable-target jumps --------
      // The byte target is resolved at runtime by indexing byte_to_insn[vars[op1]].
      // We tag the family in `aux` so the kernel can dispatch without a separate
      // opcode per (condition, abs/rel) combination -- 8 opcodes collapsing to 1
      // helps device-side branch prediction.
      case OpCode::RelativeJumpToVariableAddressIfVariableGt0:
      case OpCode::RelativeJumpToVariableAddressIfVariableLt0:
      case OpCode::RelativeJumpToVariableAddressIfVariableEq0:
      case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0:
      case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0:
      case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: {
        const int32_t cond_var = readInt32LE(program_data, body);
        const int32_t addr_var = readInt32LE(program_data, body + 4 + 1);
        VarJumpKind kind = kVarJumpIfEq0Abs;
        switch (span.opcode) {
          case OpCode::RelativeJumpToVariableAddressIfVariableGt0: kind = kVarJumpIfGt0Rel; break;
          case OpCode::RelativeJumpToVariableAddressIfVariableLt0: kind = kVarJumpIfLt0Rel; break;
          case OpCode::RelativeJumpToVariableAddressIfVariableEq0: kind = kVarJumpIfEq0Rel; break;
          case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: kind = kVarJumpIfGt0Abs; break;
          case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: kind = kVarJumpIfLt0Abs; break;
          case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: kind = kVarJumpIfEq0Abs; break;
          default: break;
        }
        insn.opcode = Op_VarTargetJump;
        insn.aux = static_cast<uint16_t>(kind);
        insn.op0 = static_cast<int32_t>(clampVarIndex(cond_var, variable_count));
        insn.op1 = static_cast<int32_t>(clampVarIndex(addr_var, variable_count));
        break;
      }
      case OpCode::UnconditionalJumpToAbsoluteVariableAddress:
      case OpCode::UnconditionalJumpToRelativeVariableAddress: {
        const int32_t addr_var = readInt32LE(program_data, body);
        insn.opcode = Op_VarTargetJump;
        insn.aux = static_cast<uint16_t>(
            span.opcode == OpCode::UnconditionalJumpToAbsoluteVariableAddress
                ? kVarJumpAlwaysAbs
                : kVarJumpAlwaysRel);
        insn.op0 = 0;
        insn.op1 = static_cast<int32_t>(clampVarIndex(addr_var, variable_count));
        break;
      }

      // -------- Tier 3: subroutines --------
      // Wire format mirrors `Program::callSubroutine`:
      //   byte 0    : opcode 0x4d
      //   byte 1    : subroutine_id (uint8)
      //   byte 2    : input_arity   (uint8)
      //   byte 3    : output_arity  (uint8)
      //   byte 4..  : (int32 variable_index, int8 follow_flag) per input
      //   byte ..   : (int32 variable_index, int8 follow_flag) per output
      // We pull the args into the per-program `call_subroutine_args` table and store
      // the table offset in `aux` so the kernel can find them without re-parsing.
      case OpCode::CallSubroutine: {
        const uint8_t subroutine_id = program_data[body];
        const uint8_t in_arity = program_data[body + 1];
        const uint8_t out_arity = program_data[body + 2];
        const uint32_t arg_count = static_cast<uint32_t>(in_arity) + out_arity;
        const uint32_t base = static_cast<uint32_t>(out.call_subroutine_args.size());
        // Each (var, follow) arg pair occupies 5 bytes on the wire; we don't care
        // about the canonical `kArgPairSize` constant from the program parser here,
        // we mirror it locally to keep the GPU compiler dependency-light.
        constexpr uint32_t kArgPairSize = 4U + 1U;
        const uint32_t args_offset = body + 3;
        for (uint32_t i = 0; i < arg_count; ++i) {
          CompiledCallArg arg;
          arg.variable_index = static_cast<int32_t>(clampVarIndex(
              readInt32LE(program_data, args_offset + i * kArgPairSize), variable_count));
          arg.follow_flag = static_cast<uint8_t>(
              readInt8(program_data, args_offset + i * kArgPairSize + 4) != 0 ? 1 : 0);
          out.call_subroutine_args.push_back(arg);
        }
        // We can't fit a 16-bit base offset for programs with many subroutine calls,
        // so refuse to compile and let the hybrid dispatcher route to CPU. In
        // practice 65535 args is way more than anything we evolve, but it's a real
        // upper bound we should document. (Crossing it would have indicated a runaway
        // genome anyway.)
        if (base > std::numeric_limits<uint16_t>::max()) {
          insn.opcode = Op_NoOp;
          break;
        }
        insn.opcode = Op_CallSubroutine;
        insn.op0 = static_cast<int32_t>(subroutine_id);
        insn.op1 = static_cast<int32_t>(in_arity);
        insn.op2 = static_cast<int32_t>(out_arity);
        insn.aux = static_cast<uint16_t>(base);
        out.uses_subroutines = true;
        break;
      }

      // -------- Truly device-NoOp (matches DeviceNoOpEquivalent annotation) ---------
      // PrintVariable, PrintStringFromStringTable, PrintVariableStringFromStringTable,
      // PerformSystemCall: all CPU side-effects invisible to scoring. The compiler
      // emits Op_NoOp explicitly so a reader knows the intent (vs the "default:" path
      // which signals "we forgot about this opcode").
      case OpCode::PrintVariable:
      case OpCode::PrintStringFromStringTable:
      case OpCode::PrintVariableStringFromStringTable:
      case OpCode::PerformSystemCall:
        insn.opcode = Op_NoOp;
        break;

      // -------- Everything else folds to NoOp. --------
      //
      // The PC still advances one instruction per source span so jump resolution works
      // for programs that contain a mix of GPU-supported and folded opcodes. With the
      // Tier 3 coverage in place this branch should ONLY fire for unknown opcode
      // values (i.e. a corrupted byte stream); legitimate opcodes are all enumerated
      // above. Keeping it as a defensive catch-all rather than removing it.
      default:
        insn.opcode = Op_NoOp;
        break;
    }

    out.instructions.push_back(insn);
  }

  // Pass 2: back-patch jump targets. We resolve constant byte offsets to instruction
  // indices using `byte_to_insn`. If the target isn't a known instruction boundary, the
  // jump folds to NoOp -- safer than executing garbage.
  for (uint32_t i = 0; i < out.instructions.size(); ++i) {
    auto& insn = out.instructions[i];
    if (insn.opcode != Op_RelJumpIfVarGt0 && insn.opcode != Op_RelJumpIfVarLt0 &&
        insn.opcode != Op_RelJumpIfVarEq0 && insn.opcode != Op_UnconditionalJump) {
      continue;
    }
    const bool is_relative = insn.aux == 1U;
    insn.aux = 0; // clear the marker before the device sees it

    // Compute the absolute byte offset of the target.
    const uint32_t source_byte_offset = parse_result.spans[i].offset;
    const uint32_t next_byte_offset = source_byte_offset + span_byte_lengths[i];
    int64_t target_byte_signed = 0;
    if (is_relative) {
      target_byte_signed = static_cast<int64_t>(next_byte_offset) + insn.op1;
    } else {
      target_byte_signed = insn.op1;
    }
    if (target_byte_signed < 0) {
      insn.opcode = Op_NoOp;
      continue;
    }
    const uint32_t target_byte = static_cast<uint32_t>(target_byte_signed);
    if (target_byte >= byte_to_insn.size() ||
        byte_to_insn[target_byte] == kNoInstructionMapping) {
      // Either lands mid-instruction or past the end. CPU VM might execute whatever
      // happens to be there; we conservatively fold to NoOp.
      insn.opcode = Op_NoOp;
      insn.op1 = 0;
      continue;
    }
    insn.op1 = static_cast<int32_t>(byte_to_insn[target_byte]);
  }

  return out;
}

} // namespace beast::cuda
