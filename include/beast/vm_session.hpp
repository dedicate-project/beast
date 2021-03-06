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

  bool isAtEnd();

  void registerVariable(int32_t variable_index, Program::VariableType variable_type);

  int32_t getRealVariableIndex(int32_t variable_index, bool follow_links);

  void setVariable(int32_t variable_index, int32_t value, bool follow_links);

  void unregisterVariable(int32_t variable_index);

  void setStringTableEntry(int32_t string_table_index, const std::string& string_content);

  const std::string& getStringTableEntry(int32_t string_table_index);

  void appendToPrintBuffer(const std::string& string);

  void appendVariableToPrintBuffer(int32_t variable_index, bool follow_links);

  const std::string& getPrintBuffer() const;

  void clearPrintBuffer();

 private:
  Program program_;

  int32_t pointer_;

  size_t variable_count_;

  size_t string_table_count_;

  size_t max_string_size_;

  std::map<int32_t, std::pair<Program::VariableType, int32_t>> variables_;

  std::map<int32_t, std::string> string_table_;

  std::string print_buffer_;
};

}  // namespace beast

#endif  // BEAST_VM_SESSION_HPP_
