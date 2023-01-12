#ifndef BEAST_OPCODES_HPP_
#define BEAST_OPCODES_HPP_

namespace beast {

/**
 * @brief Describes the available operators
 *
 * This list contains all known operators that can be used in a BEAST program. Each of them has its
 * specific encoding managed by the Program class when adding such an operator and its operands to
 * the byte code. Also, each operator is handled distinctively by the executing virtual machine
 * (e.g., a CpuVirtualMachine instance).
 *
 * The operators are separated into categories as shown below. If a new operator is added, it needs
 * to either go into an existing group that matches its semantics, or a new group should be opened.
 */
enum class OpCode : int8_t {
  // Misc
  NoOp                                       = 0x00,   ///< Perform no operation
  LoadMemorySizeIntoVariable                 = 0x01,   ///< Load the maximum variable count into a
                                                       ///  variable
  LoadCurrentAddressIntoVariable             = 0x02,   ///< Load the current execution pointer
                                                       ///  address into a variable
  Terminate                                  = 0x03,   ///< Terminate the program with a fixed
                                                       ///  return code
  TerminateWithVariableReturnCode            = 0x04,   ///< Terminate the program with a variable
                                                       ///  return code
  PerformSystemCall                          = 0x05,   ///< Perform a system call
  LoadRandomValueIntoVariable                = 0x06,   ///< Load a random value into a variable

  // Variable management
  DeclareVariable                            = 0x07,   ///< Declare a variable with index and type
  SetVariable                                = 0x08,   ///< Set the value of a variable
  UndeclareVariable                          = 0x09,   ///< Undeclare a variable
  CopyVariable                               = 0x0a,   ///< Copy the value of a variable
  SwapVariables                              = 0x0b,   ///< Swap the values of two variables

  // Math
  AddConstantToVariable                      = 0x0c,   ///< Add a constant to a variable
  AddVariableToVariable                      = 0x0d,   ///< Add a variable to a variable
  SubtractConstantFromVariable               = 0x0e,   ///< Subtract a constant from a variable
  SubtractVariableFromVariable               = 0x0f,   ///< Subtract a variable from a variable
  CompareIfVariableGtConstant                = 0x10,   ///< Compare if a variable is > a constant
  CompareIfVariableLtConstant                = 0x11,   ///< Compare if a variable is < a constant
  CompareIfVariableEqConstant                = 0x12,   ///< Compare if a variable is = a constant
  CompareIfVariableGtVariable                = 0x13,   ///< Compare if a variable is > a variable
  CompareIfVariableLtVariable                = 0x14,   ///< Compare if a variable is < a variable
  CompareIfVariableEqVariable                = 0x15,   ///< Compare if a variable is = a variable
  GetMaxOfVariableAndConstant                = 0x16,   ///< Get the maximum of a variable and a
                                                       ///  constant
  GetMinOfVariableAndConstant                = 0x17,   ///< Get the minimum of a variable and a
                                                       ///  constant
  GetMaxOfVariableAndVariable                = 0x18,   ///< Get the maximum of a variable and a
                                                       ///  variable
  GetMinOfVariableAndVariable                = 0x19,   ///< Get the minimum of a variable and a
                                                       ///  variable
  ModuloVariableByConstant                   = 0x1a,   ///< Modulo a variable by a constant
  ModuloVariableByVariable                   = 0x1b,   ///< Modulo a variable by a variable

  // Bit manipulation
  BitShiftVariableLeft                       = 0x1c,   ///< Bit shift a variable to the left
  BitShiftVariableRight                      = 0x1d,   ///< Bit shift a variable to the right
  BitWiseInvertVariable                      = 0x1e,   ///< Bit-wise invert a variable
  BitWiseAndTwoVariables                     = 0x1f,   ///< Bit-wise AND two variables
  BitWiseOrTwoVariables                      = 0x20,   ///< Bit-wise OR two variables
  BitWiseXorTwoVariables                     = 0x21,   ///< Bit-wise XOR two variables
  RotateVariableLeft                         = 0x22,   ///< Rotate a variable to the left
  RotateVariableRight                        = 0x23,   ///< Rotate a variable to the right
  VariableBitShiftVariableLeft               = 0x24,   ///< Variably bit shift a variable to the
                                                       ///  left
  VariableBitShiftVariableRight              = 0x25,   ///< Variably bit shift a variable to the
                                                       ///  right
  VariableRotateVariableLeft                 = 0x26,   ///< Variably rotate a variable to the left
  VariableRotateVariableRight                = 0x27,   ///< Variably rotate a variable to the right

  // Jumps
  RelativeJumpToVariableAddressIfVariableGt0 = 0x28,   ///< Perform a relative jump to a variable
                                                       ///  address if a variable is > 0
  RelativeJumpToVariableAddressIfVariableLt0 = 0x29,   ///< Perform a relative jump to a variable
                                                       ///  address if a variable is < 0
  RelativeJumpToVariableAddressIfVariableEq0 = 0x2a,   ///< Perform a relative jump to a variable
                                                       ///  address if a variable is = 0
  AbsoluteJumpToVariableAddressIfVariableGt0 = 0x2b,   ///< Perform an absolute jump to a variable
                                                       ///  address if a variable is > 0
  AbsoluteJumpToVariableAddressIfVariableLt0 = 0x2c,   ///< Perform an absolute jump to a variable
                                                       ///  address if a variable is < 0
  AbsoluteJumpToVariableAddressIfVariableEq0 = 0x2d,   ///< Perform an absolute jump to a variable
                                                       ///  address if a variable is = 0
  RelativeJumpIfVariableGt0                  = 0x2e,   ///< Perform a relative jump if a variable is
                                                       ///  > 0
  RelativeJumpIfVariableLt0                  = 0x2f,   ///< Perform a relative jump if a variable is
                                                       ///  < 0
  RelativeJumpIfVariableEq0                  = 0x30,   ///< Perform a relative jump if a variable is
                                                       ///  = 0
  AbsoluteJumpIfVariableGt0                  = 0x31,   ///< Perform an absolute jump if a variable
                                                       ///  is > 0
  AbsoluteJumpIfVariableLt0                  = 0x32,   ///< Perform an absolute jump if a variable
                                                       ///  is < 0
  AbsoluteJumpIfVariableEq0                  = 0x33,   ///< Perform an absolute jump if a variable
                                                       ///  is = 0
  UnconditionalJumpToAbsoluteAddress         = 0x34,   ///< Perform an unconditional jump to an
                                                       ///  absolute address
  UnconditionalJumpToAbsoluteVariableAddress = 0x35,   ///< Perform an unconditional jump to an
                                                       ///  absolute variable address
  UnconditionalJumpToRelativeAddress         = 0x36,   ///< Perform an unconditional jump to a
                                                       ///  relative address
  UnconditionalJumpToRelativeVariableAddress = 0x37,   ///< Perform an unconditional jump to a
                                                       ///  relative variable address

  // I/O
  CheckIfVariableIsInput                     = 0x38,   ///< Check if a variable is registered as an
                                                       ///  input
  CheckIfVariableIsOutput                    = 0x39,   ///< Check if a variable is registered as an
                                                       ///  output
  LoadInputCountIntoVariable                 = 0x3a,   ///< Load the number of registered inputs
                                                       ///  into a variable
  LoadOutputCountIntoVariable                = 0x3b,   ///< Load the number of registered outputs
                                                       ///  into a variable
  CheckIfInputWasSet                         = 0x3c,   ///< Check if an iput variable was set since
                                                       ///  the last read operation

  // Printing and string table
  PrintVariable                              = 0x3d,   ///< Print a variable's value
  SetStringTableEntry                        = 0x3e,   ///< Set an entry in the string table
  PrintStringFromStringTable                 = 0x3f,   ///< Print an entry from the string table
  LoadStringTableLimitIntoVariable           = 0x40,   ///< Load maximum size of string table into a
                                                       ///  variable
  LoadStringTableItemLengthLimitIntoVariable = 0x41,   ///< Load the maximum length of an individual
                                                       ///  string item into a variable
  SetVariableStringTableEntry                = 0x42,   ///< Set an entry in the string table, based
                                                       ///  on a variable index
  PrintVariableStringFromStringTable         = 0x43,   ///< Print an entry from the string table,
                                                       ///  based on a variable index
  LoadVariableStringItemLengthIntoVariable   = 0x44,   ///< Load the length of a specific string
                                                       ///  table item into a variable, based on a
                                                       ///  variable index
  LoadVariableStringItemIntoVariables        = 0x45,   ///< Load a string table entry into a
                                                       ///  sequence of variables, based in a
                                                       ///  variable index
  LoadStringItemLengthIntoVariable           = 0x46,   ///< Load the length of a specific string
                                                       ///  table item into a variable
  LoadStringItemIntoVariables                = 0x47,   ///< Load a string table entry into a
                                                       ///  sequence of variables

  // Stack
  PushVariableOnStack                        = 0x48,   ///< Push the value of a variable onto a
                                                       ///  stack
  PushConstantOnStack                        = 0x49,   ///< Push a constant onto a stack
  PopVariableFromStack                       = 0x4a,   ///< Pop the top item from a stack into a
                                                       ///  variable
  PopTopItemFromStack                        = 0x4b,   ///< Pop the top item from a stack
  CheckIfStackIsEmpty                        = 0x4c    ///< Check if a stack contains no items
};

}  // namespace beast

#endif  // BEAST_OPCODES_HPP_
