#ifndef BEAST_PROGRAM_HPP_
#define BEAST_PROGRAM_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace beast {

class Program {
 public:
  // Denotes the type of the declared variable.
  enum class VariableType {
    Int32 = 0,  // A four byte signed integer type
    Link = 1    // A link to another variable (resolved when the variable content is accessed)
  };

  Program(int32_t space);

  int32_t getSize();

  int32_t getData4(int32_t offset);

  int16_t getData2(int32_t offset);

  int8_t getData1(int32_t offset);

  void declareVariable(int32_t variable_index, VariableType variable_type);

  void setVariable(int32_t variable_index, int32_t content, bool follow_links);

  void undeclareVariable(int32_t variable_index);

  void addConstantToVariable(int32_t source_variable_index, int32_t destination_variable_index,
                             int32_t constant_to_add);

  void addVariableToVariable(int32_t source_variable_index, int32_t destination_variable_index,
                             int32_t variable_index_to_add);

  void subtractConstantFromVariable(
    int32_t source_variable_index, int32_t destination_variable_index,
    int32_t constant_to_subtract);

  void subtractVariableFromVariable(
    int32_t source_variable_index, int32_t destination_variable_index,
    int32_t variable_index_to_subtract);

  void relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t relative_jump_address_variable_index);

  void relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t relative_jump_address_variable_index);

  void relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t relative_jump_address_variable_index);

  void absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t absolute_jump_address_variable_index);

  void absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t absolute_jump_address_variable_index);

  void absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t absolute_jump_address_variable_index);

  void relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t relative_jump_address);

  void relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t relative_jump_address);

  void relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t relative_jump_address);

  void absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t absolute_jump_address);

  void absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t absolute_jump_address);

  void absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t absolute_jump_address);

  void loadMemorySizeIntoVariable(int32_t variable_index);

  void checkIfVariableIsInput(int32_t source_variable_index, int32_t destination_variable_index);

  void checkIfVariableIsOutput(int32_t source_variable_index, int32_t destination_variable_index);

  void loadInputCountIntoVariable(int32_t variable_index);

  void loadOutputCountIntoVariable(int32_t variable_index);

  void loadCurrentAddressIntoVariable(int32_t variable_index);

  void printVariable(int32_t variable_index, bool follow_links);

  void setStringTableEntry(int32_t string_table_index, std::string string);

  void printStringFromStringTable(int32_t string_table_index);

  void loadStringTableLimitIntoVariable(int32_t variable_index);

  void terminate(unsigned char return_code);

  void copyVariable(int32_t source_variable_index, int32_t destination_variable_index);

 private:
  bool canFit(int32_t bytes);

  void appendData4(int32_t data);

  void appendData2(int16_t data);

  void appendData1(int8_t data);

  std::vector<unsigned char> data_;
  uint32_t pointer_;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_HPP_
