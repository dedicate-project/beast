#include <beast/program_parser.hpp>

namespace beast {

bool ProgramParser::isVariableLengthOperator(OpCode opcode) noexcept {
  switch (opcode) {
  case OpCode::SetStringTableEntry:
  case OpCode::SetVariableStringTableEntry:
  case OpCode::CallSubroutine:
    return true;
  default:
    return false;
  }
}

namespace {

/// CallSubroutine encoding constants.
///
/// Wire format (matches `Program::callSubroutine`):
///   byte 0     : opcode (0x4d)
///   byte 1     : subroutine_id (uint8)
///   byte 2     : input_arity   (uint8)
///   byte 3     : output_arity  (uint8)
///   byte 4..   : (int32 variable_index, int8 follow_links) per input
///   byte ..    : (int32 variable_index, int8 follow_links) per output
constexpr uint32_t kCallSubroutineFixedPrefixBytes = 4;
constexpr uint32_t kCallSubroutineBytesPerArgument = 5;
constexpr uint32_t kCallSubroutineInputArityOffset = 2;
constexpr uint32_t kCallSubroutineOutputArityOffset = 3;

} // namespace

// Encoding cheat sheet (matches CpuVirtualMachine::step):
//   v4 = 4-byte variable index   d4 = 4-byte data         f1 = 1-byte follow flag
//   d1 = 1-byte data             d2 = 2-byte data (string length)
// The opcode itself is always 1 byte. Lengths below are computed as "1 + sum(operand sizes)".
//
// Adding a new opcode? Update this switch *and* `OpCode::Size`.
uint32_t ProgramParser::getOperatorLength(OpCode opcode) noexcept {
  switch (opcode) {
  case OpCode::NoOp:                                       return 1;
  case OpCode::LoadMemorySizeIntoVariable:                 return 1 + 4 + 1;
  case OpCode::LoadCurrentAddressIntoVariable:             return 1 + 4 + 1;
  case OpCode::Terminate:                                  return 1 + 1;
  case OpCode::TerminateWithVariableReturnCode:            return 1 + 4 + 1;
  case OpCode::PerformSystemCall:                          return 1 + 1 + 1 + 4 + 1;
  case OpCode::LoadRandomValueIntoVariable:                return 1 + 4 + 1;
  case OpCode::DeclareVariable:                            return 1 + 4 + 1;
  case OpCode::SetVariable:                                return 1 + 4 + 1 + 4;
  case OpCode::UndeclareVariable:                          return 1 + 4;
  case OpCode::CopyVariable:                               return 1 + 4 + 1 + 4 + 1;
  case OpCode::SwapVariables:                              return 1 + 4 + 1 + 4 + 1;
  case OpCode::AddConstantToVariable:                      return 1 + 4 + 1 + 4;
  case OpCode::AddVariableToVariable:                      return 1 + 4 + 1 + 4 + 1;
  case OpCode::SubtractConstantFromVariable:               return 1 + 4 + 1 + 4;
  case OpCode::SubtractVariableFromVariable:               return 1 + 4 + 1 + 4 + 1;
  case OpCode::CompareIfVariableGtConstant:                return 1 + 4 + 1 + 4 + 4 + 1;
  case OpCode::CompareIfVariableLtConstant:                return 1 + 4 + 1 + 4 + 4 + 1;
  case OpCode::CompareIfVariableEqConstant:                return 1 + 4 + 1 + 4 + 4 + 1;
  case OpCode::CompareIfVariableGtVariable:                return 1 + 4 + 1 + 4 + 1 + 4 + 1;
  case OpCode::CompareIfVariableLtVariable:                return 1 + 4 + 1 + 4 + 1 + 4 + 1;
  case OpCode::CompareIfVariableEqVariable:                return 1 + 4 + 1 + 4 + 1 + 4 + 1;
  case OpCode::GetMaxOfVariableAndConstant:                return 1 + 4 + 1 + 4 + 4 + 1;
  case OpCode::GetMinOfVariableAndConstant:                return 1 + 4 + 1 + 4 + 4 + 1;
  case OpCode::GetMaxOfVariableAndVariable:                return 1 + 4 + 1 + 4 + 1 + 4 + 1;
  case OpCode::GetMinOfVariableAndVariable:                return 1 + 4 + 1 + 4 + 1 + 4 + 1;
  case OpCode::ModuloVariableByConstant:                   return 1 + 4 + 1 + 4;
  case OpCode::ModuloVariableByVariable:                   return 1 + 4 + 1 + 4 + 1;
  case OpCode::BitShiftVariableLeft:                       return 1 + 4 + 1 + 1;
  case OpCode::BitShiftVariableRight:                      return 1 + 4 + 1 + 1;
  case OpCode::BitWiseInvertVariable:                      return 1 + 4 + 1;
  case OpCode::BitWiseAndTwoVariables:                     return 1 + 4 + 1 + 4 + 1;
  case OpCode::BitWiseOrTwoVariables:                      return 1 + 4 + 1 + 4 + 1;
  case OpCode::BitWiseXorTwoVariables:                     return 1 + 4 + 1 + 4 + 1;
  case OpCode::RotateVariableLeft:                         return 1 + 4 + 1 + 1;
  case OpCode::RotateVariableRight:                        return 1 + 4 + 1 + 1;
  case OpCode::VariableBitShiftVariableLeft:               return 1 + 4 + 1 + 4 + 1;
  case OpCode::VariableBitShiftVariableRight:              return 1 + 4 + 1 + 4 + 1;
  case OpCode::VariableRotateVariableLeft:                 return 1 + 4 + 1 + 4 + 1;
  case OpCode::VariableRotateVariableRight:                return 1 + 4 + 1 + 4 + 1;
  case OpCode::RelativeJumpToVariableAddressIfVariableGt0: return 1 + 4 + 1 + 4 + 1;
  case OpCode::RelativeJumpToVariableAddressIfVariableLt0: return 1 + 4 + 1 + 4 + 1;
  case OpCode::RelativeJumpToVariableAddressIfVariableEq0: return 1 + 4 + 1 + 4 + 1;
  case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: return 1 + 4 + 1 + 4 + 1;
  case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: return 1 + 4 + 1 + 4 + 1;
  case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: return 1 + 4 + 1 + 4 + 1;
  case OpCode::RelativeJumpIfVariableGt0:                  return 1 + 4 + 1 + 4;
  case OpCode::RelativeJumpIfVariableLt0:                  return 1 + 4 + 1 + 4;
  case OpCode::RelativeJumpIfVariableEq0:                  return 1 + 4 + 1 + 4;
  case OpCode::AbsoluteJumpIfVariableGt0:                  return 1 + 4 + 1 + 4;
  case OpCode::AbsoluteJumpIfVariableLt0:                  return 1 + 4 + 1 + 4;
  case OpCode::AbsoluteJumpIfVariableEq0:                  return 1 + 4 + 1 + 4;
  case OpCode::UnconditionalJumpToAbsoluteAddress:         return 1 + 4;
  case OpCode::UnconditionalJumpToAbsoluteVariableAddress: return 1 + 4 + 1;
  case OpCode::UnconditionalJumpToRelativeAddress:         return 1 + 4;
  case OpCode::UnconditionalJumpToRelativeVariableAddress: return 1 + 4 + 1;
  case OpCode::CheckIfVariableIsInput:                     return 1 + 4 + 1 + 4 + 1;
  case OpCode::CheckIfVariableIsOutput:                    return 1 + 4 + 1 + 4 + 1;
  case OpCode::LoadInputCountIntoVariable:                 return 1 + 4 + 1;
  case OpCode::LoadOutputCountIntoVariable:                return 1 + 4 + 1;
  case OpCode::CheckIfInputWasSet:                         return 1 + 4 + 1 + 4 + 1;
  case OpCode::PrintVariable:                              return 1 + 4 + 1 + 1;
  case OpCode::SetStringTableEntry:                        return 1 + 4 + 2;
  case OpCode::PrintStringFromStringTable:                 return 1 + 4;
  case OpCode::LoadStringTableLimitIntoVariable:           return 1 + 4 + 1;
  case OpCode::LoadStringTableItemLengthLimitIntoVariable: return 1 + 4 + 1;
  case OpCode::SetVariableStringTableEntry:                return 1 + 4 + 1 + 2;
  case OpCode::PrintVariableStringFromStringTable:         return 1 + 4 + 1;
  case OpCode::LoadVariableStringItemLengthIntoVariable:   return 1 + 4 + 1 + 4 + 1;
  case OpCode::LoadVariableStringItemIntoVariables:        return 1 + 4 + 1 + 4 + 1;
  case OpCode::LoadStringItemLengthIntoVariable:           return 1 + 4 + 4 + 1;
  case OpCode::LoadStringItemIntoVariables:                return 1 + 4 + 4 + 1;
  case OpCode::PushVariableOnStack:                        return 1 + 4 + 1 + 4 + 1;
  case OpCode::PushConstantOnStack:                        return 1 + 4 + 1 + 4;
  case OpCode::PopVariableFromStack:                       return 1 + 4 + 1 + 4 + 1;
  case OpCode::PopTopItemFromStack:                        return 1 + 4 + 1;
  case OpCode::CheckIfStackIsEmpty:                        return 1 + 4 + 1 + 4 + 1;
  case OpCode::CallSubroutine:                             return kCallSubroutineFixedPrefixBytes;
  case OpCode::Size:                                       return 0;
  }
  return 0;
}

ProgramParser::ParseResult ProgramParser::parse(const std::vector<unsigned char>& data) noexcept {
  ParseResult result;
  uint32_t offset = 0;
  const uint32_t total = static_cast<uint32_t>(data.size());

  while (offset < total) {
    const auto opcode_byte = static_cast<int8_t>(data[offset]);
    const auto opcode = static_cast<OpCode>(opcode_byte);

    const uint32_t fixed = getOperatorLength(opcode);
    if (fixed == 0) {
      // Unknown opcode; bail out cleanly and report the tail as garbage.
      result.clean = false;
      result.trailing_garbage_bytes = total - offset;
      return result;
    }

    uint32_t span_length = fixed;
    if (isVariableLengthOperator(opcode)) {
      // We need the fixed prefix to be fully in-bounds before we can read any payload
      // length data.
      if (offset + fixed > total) {
        result.clean = false;
        result.trailing_garbage_bytes = total - offset;
        return result;
      }
      if (opcode == OpCode::CallSubroutine) {
        // Two arity bytes at fixed offsets within the prefix; payload is
        // 5 bytes per argument (int32 var_index + 1-byte follow_links flag).
        const uint8_t input_arity = data[offset + kCallSubroutineInputArityOffset];
        const uint8_t output_arity = data[offset + kCallSubroutineOutputArityOffset];
        span_length += static_cast<uint32_t>(input_arity + output_arity) *
                       kCallSubroutineBytesPerArgument;
      } else {
        // String-table operators: 2-byte little-endian length field at end of prefix,
        // payload is that many character bytes.
        const uint32_t length_offset = offset + fixed - 2;
        const uint16_t string_length = static_cast<uint16_t>(data[length_offset]) |
                                       (static_cast<uint16_t>(data[length_offset + 1]) << 8);
        span_length += string_length;
      }
    }

    if (offset + span_length > total) {
      // Operator's body extends past the end of the stream; treat the rest as garbage rather
      // than recording a half-instruction.
      result.clean = false;
      result.trailing_garbage_bytes = total - offset;
      return result;
    }

    OperatorSpan span;
    span.opcode = opcode;
    span.offset = offset;
    span.length = span_length;
    result.spans.push_back(span);
    offset += span_length;
  }

  return result;
}

} // namespace beast
