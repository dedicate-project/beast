#ifndef BEAST_VM_SESSION_HPP_
#define BEAST_VM_SESSION_HPP_

#include <cstdint>
#include <map>

#include <beast/program.hpp>

namespace beast {

class VmSession {
 public:
  VmSession(
      Program program, size_t variable_count, size_t string_table_count,
      size_t max_string_size);

  int32_t getData4();

  int16_t getData2();

  int8_t getData1();

  int32_t getVariableValue(int32_t variable_index, bool follow_links);

  bool isAtEnd();

  void registerVariable(int32_t variable_index, Program::VariableType variable_type);

  int32_t getRealVariableIndex(int32_t variable_index, bool follow_links);

  void setVariable(int32_t variable_index, int32_t value, bool follow_links);

  void unregisterVariable(int32_t variable_index);

  void setStringTableEntry(int32_t string_table_index, const std::string& string_content);

  const std::string& getStringTableEntry(int32_t string_table_index);

  void appendToPrintBuffer(const std::string& string);

  void appendVariableToPrintBuffer(int32_t variable_index, bool follow_links, bool as_char);

  const std::string& getPrintBuffer() const;

  void clearPrintBuffer();

  void terminate(int8_t return_code);

  int8_t getReturnCode() const;

  void addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links);

  void addVariableToVariable(int32_t source_variable, int32_t destination_variable, bool follow_source_links, bool follow_destination_links);

  void subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links);

  void subtractVariableFromVariable(int32_t source_variable, int32_t destination_variable, bool follow_source_links, bool follow_destination_links);

  void relativeJumpToVariableAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  void relativeJumpToVariableAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  void relativeJumpToVariableAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  void absoluteJumpToVariableAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  void absoluteJumpToVariableAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  void absoluteJumpToVariableAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  void relativeJumpToAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  void relativeJumpToAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  void relativeJumpToAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

 private:
  Program program_;

  int32_t pointer_;

  size_t variable_count_;

  size_t string_table_count_;

  size_t max_string_size_;

  std::map<int32_t, std::pair<Program::VariableType, int32_t>> variables_;

  std::map<int32_t, std::string> string_table_;

  std::string print_buffer_;

  bool was_terminated_;

  int8_t return_code_;
};

}  // namespace beast

#endif  // BEAST_VM_SESSION_HPP_
