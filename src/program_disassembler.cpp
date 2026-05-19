#include <beast/program_disassembler.hpp>

// Standard
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Internal
#include <beast/program_parser.hpp>

namespace beast {

namespace {

int8_t readI8(const std::vector<unsigned char>& data, uint32_t offset) {
  return static_cast<int8_t>(data[offset]);
}

int16_t readI16(const std::vector<unsigned char>& data, uint32_t offset) {
  return static_cast<int16_t>(
      static_cast<uint16_t>(data[offset]) |
      (static_cast<uint16_t>(data[offset + 1]) << 8U));
}

int32_t readI32(const std::vector<unsigned char>& data, uint32_t offset) {
  return static_cast<int32_t>(
      static_cast<uint32_t>(data[offset]) |
      (static_cast<uint32_t>(data[offset + 1]) << 8U) |
      (static_cast<uint32_t>(data[offset + 2]) << 16U) |
      (static_cast<uint32_t>(data[offset + 3]) << 24U));
}

std::string hexOf(const std::vector<unsigned char>& data, uint32_t offset, uint32_t length) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (uint32_t i = 0; i < length; ++i) {
    out << std::setw(2) << static_cast<unsigned>(data[offset + i]);
  }
  return out.str();
}

// Decode the operands list according to the opcode's encoding (mirrors getOperatorLength).
// Returns operands in source order. For variable-length opcodes (string-table set,
// CallSubroutine) only the fixed-prefix operands are decoded -- the payload bytes are
// summarised in the `text` field by the caller instead so we don't blow up the JSON.
std::vector<int64_t> decodeOperands(const std::vector<unsigned char>& data, uint32_t offset,
                                     OpCode opcode) {
  std::vector<int64_t> ops;
  // `cursor` skips past the opcode byte; from there we consume operand bytes left-to-right.
  uint32_t cursor = offset + 1;
  auto take_i8 = [&]() {
    int8_t v = readI8(data, cursor);
    cursor += 1;
    ops.push_back(static_cast<int64_t>(v));
  };
  auto take_i16 = [&]() {
    int16_t v = readI16(data, cursor);
    cursor += 2;
    ops.push_back(static_cast<int64_t>(v));
  };
  auto take_i32 = [&]() {
    int32_t v = readI32(data, cursor);
    cursor += 4;
    ops.push_back(static_cast<int64_t>(v));
  };
  // Operand layouts (kept in lockstep with `getOperatorLength`):
  //   v4 = int32 var index   d4 = int32 data         f1 = int8 follow flag
  //   d1 = int8 data         d2 = int16 string len   c1 = int8 byte
  switch (opcode) {
  case OpCode::NoOp:
    break;
  case OpCode::LoadMemorySizeIntoVariable:
  case OpCode::LoadCurrentAddressIntoVariable:
  case OpCode::TerminateWithVariableReturnCode:
  case OpCode::LoadRandomValueIntoVariable:
  case OpCode::DeclareVariable:
  case OpCode::LoadInputCountIntoVariable:
  case OpCode::LoadOutputCountIntoVariable:
  case OpCode::LoadStringTableLimitIntoVariable:
  case OpCode::LoadStringTableItemLengthLimitIntoVariable:
  case OpCode::BitWiseInvertVariable:
  case OpCode::PrintVariableStringFromStringTable:
  case OpCode::PopTopItemFromStack:
  case OpCode::UnconditionalJumpToAbsoluteVariableAddress:
  case OpCode::UnconditionalJumpToRelativeVariableAddress:
    take_i32();
    take_i8();
    break;
  case OpCode::Terminate:
    take_i8();
    break;
  case OpCode::PerformSystemCall:
    take_i8();
    take_i8();
    take_i32();
    take_i8();
    break;
  case OpCode::SetVariable:
  case OpCode::AddConstantToVariable:
  case OpCode::SubtractConstantFromVariable:
  case OpCode::ModuloVariableByConstant:
    take_i32();
    take_i8();
    take_i32();
    break;
  case OpCode::UndeclareVariable:
  case OpCode::UnconditionalJumpToAbsoluteAddress:
  case OpCode::UnconditionalJumpToRelativeAddress:
  case OpCode::PrintStringFromStringTable:
    take_i32();
    break;
  case OpCode::CopyVariable:
  case OpCode::SwapVariables:
  case OpCode::AddVariableToVariable:
  case OpCode::SubtractVariableFromVariable:
  case OpCode::BitWiseAndTwoVariables:
  case OpCode::BitWiseOrTwoVariables:
  case OpCode::BitWiseXorTwoVariables:
  case OpCode::ModuloVariableByVariable:
  case OpCode::VariableBitShiftVariableLeft:
  case OpCode::VariableBitShiftVariableRight:
  case OpCode::VariableRotateVariableLeft:
  case OpCode::VariableRotateVariableRight:
  case OpCode::RelativeJumpToVariableAddressIfVariableGt0:
  case OpCode::RelativeJumpToVariableAddressIfVariableLt0:
  case OpCode::RelativeJumpToVariableAddressIfVariableEq0:
  case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0:
  case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0:
  case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0:
  case OpCode::CheckIfVariableIsInput:
  case OpCode::CheckIfVariableIsOutput:
  case OpCode::CheckIfInputWasSet:
  case OpCode::PushVariableOnStack:
  case OpCode::PopVariableFromStack:
  case OpCode::CheckIfStackIsEmpty:
  case OpCode::LoadVariableStringItemLengthIntoVariable:
  case OpCode::LoadVariableStringItemIntoVariables:
    take_i32();
    take_i8();
    take_i32();
    take_i8();
    break;
  case OpCode::CompareIfVariableGtConstant:
  case OpCode::CompareIfVariableLtConstant:
  case OpCode::CompareIfVariableEqConstant:
  case OpCode::GetMaxOfVariableAndConstant:
  case OpCode::GetMinOfVariableAndConstant:
  case OpCode::PushConstantOnStack:
    take_i32();
    take_i8();
    take_i32();
    take_i32(); // either constant or destination (op-specific) -- the caller's text rendering
                // disambiguates per-opcode, the raw integer is the same
    break;
  case OpCode::CompareIfVariableGtVariable:
  case OpCode::CompareIfVariableLtVariable:
  case OpCode::CompareIfVariableEqVariable:
  case OpCode::GetMaxOfVariableAndVariable:
  case OpCode::GetMinOfVariableAndVariable:
    take_i32();
    take_i8();
    take_i32();
    take_i8();
    take_i32();
    take_i8();
    break;
  case OpCode::BitShiftVariableLeft:
  case OpCode::BitShiftVariableRight:
  case OpCode::RotateVariableLeft:
  case OpCode::RotateVariableRight:
    take_i32();
    take_i8();
    take_i8();
    break;
  case OpCode::RelativeJumpIfVariableGt0:
  case OpCode::RelativeJumpIfVariableLt0:
  case OpCode::RelativeJumpIfVariableEq0:
  case OpCode::AbsoluteJumpIfVariableGt0:
  case OpCode::AbsoluteJumpIfVariableLt0:
  case OpCode::AbsoluteJumpIfVariableEq0:
    take_i32();
    take_i8();
    take_i32();
    break;
  case OpCode::PrintVariable:
    take_i32();
    take_i8();
    take_i8();
    break;
  case OpCode::SetStringTableEntry:
    take_i32();
    take_i16();
    break;
  case OpCode::SetVariableStringTableEntry:
    take_i32();
    take_i8();
    take_i16();
    break;
  case OpCode::LoadStringItemLengthIntoVariable:
  case OpCode::LoadStringItemIntoVariables:
    take_i32();
    take_i32();
    take_i8();
    break;
  case OpCode::CallSubroutine: {
    // Fixed prefix only: subroutine_id (u8) + input_arity (u8) + output_arity (u8). The
    // per-argument payload bytes after the prefix are summarised in the `text` field by
    // the caller, not decoded here.
    take_i8(); // subroutine id
    take_i8(); // input arity
    take_i8(); // output arity
    break;
  }
  case OpCode::Size:
    break;
  }
  return ops;
}

// Compose a pretty-printed instruction text. We deliberately render every operand inline
// (rather than relying on a separate operand list) so the UI's instruction column is
// self-contained -- "SetVariable v=0 value=42" reads better than "SetVariable" + a sibling
// operands column the user has to mentally line up.
std::string renderText(OpCode opcode, const std::vector<int64_t>& ops, const std::string& mnemonic) {
  std::ostringstream out;
  out << mnemonic;
  if (ops.empty()) {
    return out.str();
  }
  // Per-opcode field naming. Default fallback ("op0", "op1") is used for opcodes we
  // haven't bothered to give friendly names -- still functional, just less self-documenting.
  switch (opcode) {
  case OpCode::SetVariable:
    out << " v=" << ops[0] << " follow=" << ops[1] << " value=" << ops[2];
    break;
  case OpCode::AddConstantToVariable:
  case OpCode::SubtractConstantFromVariable:
  case OpCode::ModuloVariableByConstant:
    out << " v=" << ops[0] << " follow=" << ops[1] << " constant=" << ops[2];
    break;
  case OpCode::CopyVariable:
  case OpCode::SwapVariables:
  case OpCode::AddVariableToVariable:
  case OpCode::SubtractVariableFromVariable:
  case OpCode::ModuloVariableByVariable:
  case OpCode::BitWiseAndTwoVariables:
  case OpCode::BitWiseOrTwoVariables:
  case OpCode::BitWiseXorTwoVariables:
  case OpCode::VariableBitShiftVariableLeft:
  case OpCode::VariableBitShiftVariableRight:
  case OpCode::VariableRotateVariableLeft:
  case OpCode::VariableRotateVariableRight:
    out << " a=" << ops[0] << " follow_a=" << ops[1]
        << " b=" << ops[2] << " follow_b=" << ops[3];
    break;
  case OpCode::CompareIfVariableGtConstant:
  case OpCode::CompareIfVariableLtConstant:
  case OpCode::CompareIfVariableEqConstant:
  case OpCode::GetMaxOfVariableAndConstant:
  case OpCode::GetMinOfVariableAndConstant:
    out << " v=" << ops[0] << " follow=" << ops[1]
        << " constant=" << ops[2] << " dest=" << ops[3];
    break;
  case OpCode::CompareIfVariableGtVariable:
  case OpCode::CompareIfVariableLtVariable:
  case OpCode::CompareIfVariableEqVariable:
  case OpCode::GetMaxOfVariableAndVariable:
  case OpCode::GetMinOfVariableAndVariable:
    out << " a=" << ops[0] << " follow_a=" << ops[1]
        << " b=" << ops[2] << " follow_b=" << ops[3]
        << " dest=" << ops[4] << " follow_dest=" << ops[5];
    break;
  case OpCode::BitShiftVariableLeft:
  case OpCode::BitShiftVariableRight:
  case OpCode::RotateVariableLeft:
  case OpCode::RotateVariableRight:
    out << " v=" << ops[0] << " follow=" << ops[1] << " bits=" << ops[2];
    break;
  case OpCode::UnconditionalJumpToAbsoluteAddress:
  case OpCode::UnconditionalJumpToRelativeAddress:
    out << " address=" << ops[0];
    break;
  case OpCode::Terminate:
    out << " return_code=" << ops[0];
    break;
  case OpCode::CallSubroutine:
    out << " id=" << ops[0] << " input_arity=" << ops[1] << " output_arity=" << ops[2];
    break;
  default:
    for (size_t i = 0; i < ops.size(); ++i) {
      out << " op" << i << "=" << ops[i];
    }
    break;
  }
  return out.str();
}

} // namespace

std::string ProgramDisassembler::mnemonicFor(OpCode opcode) {
  // Keep the mnemonic table colocated with the disassembler -- it's the only consumer
  // and adding new opcodes only requires touching one switch in this file plus the
  // existing one in ProgramParser::getOperatorLength.
  switch (opcode) {
  case OpCode::NoOp:                                       return "NoOp";
  case OpCode::LoadMemorySizeIntoVariable:                 return "LoadMemorySizeIntoVariable";
  case OpCode::LoadCurrentAddressIntoVariable:             return "LoadCurrentAddressIntoVariable";
  case OpCode::Terminate:                                  return "Terminate";
  case OpCode::TerminateWithVariableReturnCode:            return "TerminateWithVariableReturnCode";
  case OpCode::PerformSystemCall:                          return "PerformSystemCall";
  case OpCode::LoadRandomValueIntoVariable:                return "LoadRandomValueIntoVariable";
  case OpCode::DeclareVariable:                            return "DeclareVariable";
  case OpCode::SetVariable:                                return "SetVariable";
  case OpCode::UndeclareVariable:                          return "UndeclareVariable";
  case OpCode::CopyVariable:                               return "CopyVariable";
  case OpCode::SwapVariables:                              return "SwapVariables";
  case OpCode::AddConstantToVariable:                      return "AddConstantToVariable";
  case OpCode::AddVariableToVariable:                      return "AddVariableToVariable";
  case OpCode::SubtractConstantFromVariable:               return "SubtractConstantFromVariable";
  case OpCode::SubtractVariableFromVariable:               return "SubtractVariableFromVariable";
  case OpCode::CompareIfVariableGtConstant:                return "CompareIfVariableGtConstant";
  case OpCode::CompareIfVariableLtConstant:                return "CompareIfVariableLtConstant";
  case OpCode::CompareIfVariableEqConstant:                return "CompareIfVariableEqConstant";
  case OpCode::CompareIfVariableGtVariable:                return "CompareIfVariableGtVariable";
  case OpCode::CompareIfVariableLtVariable:                return "CompareIfVariableLtVariable";
  case OpCode::CompareIfVariableEqVariable:                return "CompareIfVariableEqVariable";
  case OpCode::GetMaxOfVariableAndConstant:                return "GetMaxOfVariableAndConstant";
  case OpCode::GetMinOfVariableAndConstant:                return "GetMinOfVariableAndConstant";
  case OpCode::GetMaxOfVariableAndVariable:                return "GetMaxOfVariableAndVariable";
  case OpCode::GetMinOfVariableAndVariable:                return "GetMinOfVariableAndVariable";
  case OpCode::ModuloVariableByConstant:                   return "ModuloVariableByConstant";
  case OpCode::ModuloVariableByVariable:                   return "ModuloVariableByVariable";
  case OpCode::BitShiftVariableLeft:                       return "BitShiftVariableLeft";
  case OpCode::BitShiftVariableRight:                      return "BitShiftVariableRight";
  case OpCode::BitWiseInvertVariable:                      return "BitWiseInvertVariable";
  case OpCode::BitWiseAndTwoVariables:                     return "BitWiseAndTwoVariables";
  case OpCode::BitWiseOrTwoVariables:                      return "BitWiseOrTwoVariables";
  case OpCode::BitWiseXorTwoVariables:                     return "BitWiseXorTwoVariables";
  case OpCode::RotateVariableLeft:                         return "RotateVariableLeft";
  case OpCode::RotateVariableRight:                        return "RotateVariableRight";
  case OpCode::VariableBitShiftVariableLeft:               return "VariableBitShiftVariableLeft";
  case OpCode::VariableBitShiftVariableRight:              return "VariableBitShiftVariableRight";
  case OpCode::VariableRotateVariableLeft:                 return "VariableRotateVariableLeft";
  case OpCode::VariableRotateVariableRight:                return "VariableRotateVariableRight";
  case OpCode::RelativeJumpToVariableAddressIfVariableGt0: return "RelativeJumpToVariableAddressIfVariableGt0";
  case OpCode::RelativeJumpToVariableAddressIfVariableLt0: return "RelativeJumpToVariableAddressIfVariableLt0";
  case OpCode::RelativeJumpToVariableAddressIfVariableEq0: return "RelativeJumpToVariableAddressIfVariableEq0";
  case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: return "AbsoluteJumpToVariableAddressIfVariableGt0";
  case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: return "AbsoluteJumpToVariableAddressIfVariableLt0";
  case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: return "AbsoluteJumpToVariableAddressIfVariableEq0";
  case OpCode::RelativeJumpIfVariableGt0:                  return "RelativeJumpIfVariableGt0";
  case OpCode::RelativeJumpIfVariableLt0:                  return "RelativeJumpIfVariableLt0";
  case OpCode::RelativeJumpIfVariableEq0:                  return "RelativeJumpIfVariableEq0";
  case OpCode::AbsoluteJumpIfVariableGt0:                  return "AbsoluteJumpIfVariableGt0";
  case OpCode::AbsoluteJumpIfVariableLt0:                  return "AbsoluteJumpIfVariableLt0";
  case OpCode::AbsoluteJumpIfVariableEq0:                  return "AbsoluteJumpIfVariableEq0";
  case OpCode::UnconditionalJumpToAbsoluteAddress:         return "UnconditionalJumpToAbsoluteAddress";
  case OpCode::UnconditionalJumpToAbsoluteVariableAddress: return "UnconditionalJumpToAbsoluteVariableAddress";
  case OpCode::UnconditionalJumpToRelativeAddress:         return "UnconditionalJumpToRelativeAddress";
  case OpCode::UnconditionalJumpToRelativeVariableAddress: return "UnconditionalJumpToRelativeVariableAddress";
  case OpCode::CheckIfVariableIsInput:                     return "CheckIfVariableIsInput";
  case OpCode::CheckIfVariableIsOutput:                    return "CheckIfVariableIsOutput";
  case OpCode::LoadInputCountIntoVariable:                 return "LoadInputCountIntoVariable";
  case OpCode::LoadOutputCountIntoVariable:                return "LoadOutputCountIntoVariable";
  case OpCode::CheckIfInputWasSet:                         return "CheckIfInputWasSet";
  case OpCode::PrintVariable:                              return "PrintVariable";
  case OpCode::SetStringTableEntry:                        return "SetStringTableEntry";
  case OpCode::PrintStringFromStringTable:                 return "PrintStringFromStringTable";
  case OpCode::LoadStringTableLimitIntoVariable:           return "LoadStringTableLimitIntoVariable";
  case OpCode::LoadStringTableItemLengthLimitIntoVariable: return "LoadStringTableItemLengthLimitIntoVariable";
  case OpCode::SetVariableStringTableEntry:                return "SetVariableStringTableEntry";
  case OpCode::PrintVariableStringFromStringTable:         return "PrintVariableStringFromStringTable";
  case OpCode::LoadVariableStringItemLengthIntoVariable:   return "LoadVariableStringItemLengthIntoVariable";
  case OpCode::LoadVariableStringItemIntoVariables:        return "LoadVariableStringItemIntoVariables";
  case OpCode::LoadStringItemLengthIntoVariable:           return "LoadStringItemLengthIntoVariable";
  case OpCode::LoadStringItemIntoVariables:                return "LoadStringItemIntoVariables";
  case OpCode::PushVariableOnStack:                        return "PushVariableOnStack";
  case OpCode::PushConstantOnStack:                        return "PushConstantOnStack";
  case OpCode::PopVariableFromStack:                       return "PopVariableFromStack";
  case OpCode::PopTopItemFromStack:                        return "PopTopItemFromStack";
  case OpCode::CheckIfStackIsEmpty:                        return "CheckIfStackIsEmpty";
  case OpCode::CallSubroutine:                             return "CallSubroutine";
  case OpCode::Size:                                       return "Size";
  }
  // Unrecognised opcode value. Surface the raw byte so the UI can still display it
  // meaningfully (and the user can grep the source for it).
  std::ostringstream out;
  out << "opcode_0x" << std::hex << std::setw(2) << std::setfill('0')
      << (static_cast<unsigned>(static_cast<uint8_t>(static_cast<int8_t>(opcode))) & 0xFFU);
  return out.str();
}

ProgramDisassembler::Result
ProgramDisassembler::disassemble(const std::vector<unsigned char>& bytecode) noexcept {
  Result result;
  // Reuse the existing parser so the byte-walking logic (variable-length payloads,
  // truncation handling) stays in one place.
  const auto parsed = ProgramParser::parse(bytecode);
  result.trailing_garbage_bytes = parsed.trailing_garbage_bytes;
  result.clean = parsed.clean;
  result.instructions.reserve(parsed.spans.size());

  for (const auto& span : parsed.spans) {
    Instruction inst;
    inst.opcode = span.opcode;
    inst.offset = span.offset;
    inst.length = span.length;
    inst.mnemonic = mnemonicFor(span.opcode);
    try {
      inst.operands = decodeOperands(bytecode, span.offset, span.opcode);
    } catch (...) {
      // decodeOperands is defensive and shouldn't throw, but if it ever does we leave
      // operands empty rather than aborting the whole disassembly -- one bad opcode
      // shouldn't lose the rest of the program for the user.
      inst.operands.clear();
    }
    inst.bytes_hex = hexOf(bytecode, span.offset, span.length);
    inst.text = renderText(span.opcode, inst.operands, inst.mnemonic);
    result.instructions.push_back(std::move(inst));
  }
  return result;
}

} // namespace beast
