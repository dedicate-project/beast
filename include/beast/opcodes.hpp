#ifndef BEAST_OPCODES_HPP_
#define BEAST_OPCODES_HPP_

namespace beast {

enum class OpCode : int8_t {
  NoOp                                       = 0x00,
  DeclareVariable                            = 0x01,
  SetVariable                                = 0x02,
  UndeclareVariable                          = 0x03,
  AddConstantToVariable                      = 0x04,
  AddVariableToVariable                      = 0x05,
  SubtractConstantFromVariable               = 0x06,
  SubtractVariableFromVariable               = 0x07,
  RelativeJumpToVariableAddressIfVariableGt0 = 0x08,
  RelativeJumpToVariableAddressIfVariableLt0 = 0x09,
  RelativeJumpToVariableAddressIfVariableEq0 = 0x0a,
  AbsoluteJumpToVariableAddressIfVariableGt0 = 0x0b,
  AbsoluteJumpToVariableAddressIfVariableLt0 = 0x0c,
  AbsoluteJumpToVariableAddressIfVariableEq0 = 0x0d,
  RelativeJumpIfVariableGt0                  = 0x0e,
  RelativeJumpIfVariableLt0                  = 0x0f,
  RelativeJumpIfVariableEq0                  = 0x10,
  AbsoluteJumpIfVariableGt0                  = 0x11,
  AbsoluteJumpIfVariableLt0                  = 0x12,
  AbsoluteJumpIfVariableEq0                  = 0x13,
  LoadMemorySizeIntoVariable                 = 0x14,
  CheckIfVariableIsInput                     = 0x15,
  CheckIfVariableIsOutput                    = 0x16,
  LoadInputCountIntoVariable                 = 0x17,
  LoadOutputCountIntoVariable                = 0x18,
  LoadCurrentAddressIntoVariable             = 0x19,
  PrintVariable                              = 0x1a,
  SetStringTableEntry                        = 0x1b,
  PrintStringFromStringTable                 = 0x1c,
  LoadStringTableLimitIntoVariable           = 0x1d,
  Terminate                                  = 0x1e,
  CopyVariable                               = 0x1f,
  LoadStringItemLengthIntoVariable           = 0x20,
  LoadStringItemIntoVariables                = 0x21,
  PerformSystemCall                          = 0x22,
  BitShiftVariableLeft                       = 0x23,
  BitShiftVariableRight                      = 0x24,
  BitWiseInvertVariable                      = 0x25,
  BitWiseAndTwoVariables                     = 0x26,
  BitWiseOrTwoVariables                      = 0x27,
  BitWiseXorTwoVariables                     = 0x28,
  LoadRandomValueIntoVariable                = 0x29,
  ModuloVariableByConstant                   = 0x2a,
  ModuloVariableByVariable                   = 0x2b,
  RotateVariableLeft                         = 0x2c,
  RotateVariableRight                        = 0x2d,
  UnconditionalJumpToAbsoluteAddress         = 0x2e,
  UnconditionalJumpToAbsoluteVariableAddress = 0x2f,
  UnconditionalJumpToRelativeAddress         = 0x30,
  UnconditionalJumpToRelativeVariableAddress = 0x31,
  CheckIfInputWasSet                         = 0x32,
  LoadStringTableItemLengthLimitIntoVariable = 0x33,
  PushVariableOnStack                        = 0x34,
  PushConstantOnStack                        = 0x35,
  PopVariableFromStack                       = 0x36,
  PopFromStack                               = 0x37,
  CheckIfStackIsEmpty                        = 0x38,
  SwapVariables                              = 0x39,
  SetVariableStringTableEntry                = 0x3a,
  PrintVariableStringFromStringTable         = 0x3b,
  LoadVariableStringItemLengthIntoVariable   = 0x3c,
  LoadVariableStringItemIntoVariables        = 0x3d,
  TerminateWithVariableReturnCode            = 0x3e,
  VariableBitShiftVariableLeft               = 0x3f,
  VariableBitShiftVariableRight              = 0x40,
  VariableRotateVariableLeft                 = 0x41,
  VariableRotateVariableRight                = 0x42,
  CompareIfVariableGtConstant                = 0x43,
  CompareIfVariableLtConstant                = 0x44,
  CompareIfVariableEqConstant                = 0x45,
  CompareIfVariableGtVariable                = 0x46,
  CompareIfVariableLtVariable                = 0x47,
  CompareIfVariableEqVariable                = 0x48
};

}  // namespace beast

#endif  // BEAST_OPCODES_HPP_
