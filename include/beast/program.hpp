#ifndef BEAST_PROGRAM_HPP_
#define BEAST_PROGRAM_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <beast/opcodes.hpp>

namespace beast {

class Program {
 public:
  // Denotes the type of the declared variable.
  enum class VariableType {
    Int32 = 0,  // A four byte signed integer type
    Link = 1    // A link to another variable (resolved when the variable content is accessed)
  };

  Program();

  Program(int32_t space);

  size_t getSize() const;

  int32_t getData4(int32_t offset);

  int16_t getData2(int32_t offset);

  int8_t getData1(int32_t offset);

  uint32_t getPointer() const;

  void insertProgram(Program& other);

  const std::vector<unsigned char>& getData() const;

  void noop();

  void declareVariable(int32_t variable_index, VariableType variable_type);

  void setVariable(int32_t variable_index, int32_t content, bool follow_links);

  void undeclareVariable(int32_t variable_index);

  void addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links);

  void addVariableToVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  void subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links);

  void subtractVariableFromVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  void relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  void relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  void relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  void absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  void absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  void absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  void relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  void relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  void relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  void absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  void absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  void absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  void loadMemorySizeIntoVariable(int32_t variable_index, bool follow_links);

  void checkIfVariableIsInput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  void checkIfVariableIsOutput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  void loadInputCountIntoVariable(int32_t variable_index, bool follow_links);

  void loadOutputCountIntoVariable(int32_t variable_index, bool follow_links);

  void loadCurrentAddressIntoVariable(int32_t variable_index, bool follow_links);

  void printVariable(int32_t variable_index, bool follow_links, bool as_char);

  void setStringTableEntry(int32_t string_table_index, const std::string& string);

  void printStringFromStringTable(int32_t string_table_index);

  void loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links);

  void terminate(int8_t return_code);

  void copyVariable(
      int32_t source_variable_index, bool follow_source_links,
      int32_t destination_variable_index, bool follow_destination_links);

  void loadStringItemLengthIntoVariable(int32_t string_table_index, int32_t variable_index, bool follow_links);

  void checkIfInputWasSet(
      int32_t variable_index, bool follow_links,
      int32_t destination_variable_index, bool follow_destination_links);

  void loadStringTableItemLengthLimitIntoVariable(int32_t variable_index, bool follow_links);

  void loadRandomValueIntoVariable(int32_t variable_index, bool follow_links);

  void unconditionalJumpToAbsoluteAddress(int32_t addr);

  void unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links);

  void unconditionalJumpToRelativeAddress(int32_t addr);

  void unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links);

  void loadStringItemIntoVariables(int32_t string_table_index, int32_t start_variable_index, bool follow_links);

  void performSystemCall(int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links);

  void bitShiftVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  void bitShiftVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  void bitWiseInvertVariable(int32_t variable_index, bool follow_links);

  void bitWiseAndTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  void bitWiseOrTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  void bitWiseXorTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  void moduloVariableByConstant(int32_t variable_index, bool follow_links, int32_t modulo_constant);

  void moduloVariableByVariable(int32_t variable_index, bool follow_links, int32_t modulo_variable_index, bool modulo_follow_links);

  void rotateVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  void rotateVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  void pushVariableOnStack(int32_t stack_variable_index, bool follow_links_stack, int32_t variable_index, bool follow_links);

  void pushConstantOnStack(int32_t stack_variable_index, bool follow_links_stack, int32_t constant);

  void popVariableFromStack(int32_t stack_variable_index, bool follow_links_stack, int32_t variable_index, bool follow_links);

  void popTopItemFromStack(int32_t stack_variable_index, bool follow_links_stack);

  void checkIfStackIsEmpty(int32_t stack_variable_index, bool follow_links_stack, int32_t variable_index, bool follow_links);

  void swapVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  void setVariableStringTableEntry(int32_t variable_index, bool follow_links, const std::string& string);

  void printVariableStringFromStringTable(int32_t variable_index, bool follow_links);

  void loadVariableStringItemLengthIntoVariable(int32_t string_item_variable_index, bool follow_links_string_item, int32_t variable_index, bool follow_links);

  void loadVariableStringItemIntoVariables(int32_t string_item_variable_index, bool follow_links_string_item, int32_t variable_index, bool follow_links);

  void terminateWithVariableReturnCode(int32_t variable_index, bool follow_links);

  void variableBitShiftVariableLeft(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

  void variableBitShiftVariableRight(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

  void variableRotateVariableLeft(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

  void variableRotateVariableRight(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

 private:
  bool canFit(uint32_t bytes);

  void appendData4(int32_t data);

  void appendData2(int16_t data);

  void appendData1(int8_t data);

  void appendFlag1(bool flag);

  void appendCode1(OpCode opcode);

  void ensureSize(uint32_t size);

  std::vector<unsigned char> data_;

  uint32_t pointer_;

  bool grows_dynamically_;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_HPP_
