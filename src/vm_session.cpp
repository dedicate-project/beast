#include <beast/vm_session.hpp>

#include <set>

namespace beast {

VmSession::VmSession(
    Program program, size_t variable_count, size_t string_table_count,
    size_t max_string_size)
  : program_{std::move(program)}, pointer_{0}, variable_count_{variable_count}
  , string_table_count_{string_table_count}, max_string_size_{max_string_size} {
}

int32_t VmSession::getData4() {
  int32_t data = program_.getData4(pointer_);
  pointer_ += 4;
  return data;
}

int16_t VmSession::getData2() {
  int16_t data = program_.getData2(pointer_);
  pointer_ += 2;
  return data;
}

int8_t VmSession::getData1() {
  int8_t data = program_.getData1(pointer_);
  pointer_ += 1;
  return data;
}

bool VmSession::isAtEnd() {
  return pointer_ >= program_.getSize();
}

void VmSession::registerVariable(int32_t variable_index, Program::VariableType variable_type) {
  if (variable_index < 0 || variable_index >= variable_count_) {
    throw std::runtime_error("Invalid variable index.");
  }

  if (variable_type != Program::VariableType::Int32 &&
      variable_type != Program::VariableType::Link) {
    throw std::runtime_error("Invalid declarative variable type.");
  }

  if (variables_.find(variable_index) != variables_.end()) {
    throw std::runtime_error("Variable index already declared.");
  }

  if (variables_.size() == variable_count_) {
    // No more space for variables
    throw std::runtime_error("Variables cache full.");
  }

  variables_[variable_index] = std::make_pair(variable_type, 0);
}

int32_t VmSession::getRealVariableIndex(int32_t variable_index, bool follow_links) {
  std::set<int32_t> visited_indices;
  while (variables_.find(variable_index) != variables_.end()) {
    if (variables_[variable_index].first != Program::VariableType::Link || !follow_links) {
      // This is a non-link variable, return it.
      return variable_index;
    }

    if (variables_[variable_index].first == Program::VariableType::Link) {
      if (visited_indices.find(variable_index) != visited_indices.end()) {
        throw std::runtime_error("Circular variable index link.");
      }
      visited_indices.insert(variable_index);
      variable_index = variables_[variable_index].second;
    } else {
      throw std::runtime_error("Invalid declarative variable type.");
    }
  }

  // Undeclared variables cannot be set.
  throw std::runtime_error("Variable index not declared.");
}

void VmSession::setVariable(int32_t variable_index, int32_t value, bool follow_links) {
  variables_[getRealVariableIndex(variable_index, follow_links)].second = value;
}

void VmSession::unregisterVariable(int32_t variable_index) {
  if (variable_index < 0 || variable_index >= variable_count_) {
    throw std::runtime_error("Invalid variable index.");
  }

  if (variables_.find(variable_index) == variables_.end()) {
    throw std::runtime_error("Variable index not declared, cannot undeclare.");
  }

  variables_.erase(variable_index);
}

void VmSession::setStringTableEntry(int32_t string_table_index, const std::string& string_content) {
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::runtime_error("String table index out of bounds.");
  }

  if (string_content.size() > max_string_size_) {
    throw std::runtime_error("String too long.");
  }

  string_table_[string_table_index] = string_content;
}

const std::string& VmSession::getStringTableEntry(int32_t string_table_index) {
  if (string_table_.find(string_table_index) == string_table_.end()) {
    throw std::runtime_error("String table index out of bounds.");
  }

  return string_table_[string_table_index];
}

void VmSession::appendToPrintBuffer(const std::string& string) {
  print_buffer_ += string;
}

void VmSession::appendVariableToPrintBuffer(int32_t variable_index, bool follow_links) {
  variable_index = getRealVariableIndex(variable_index, follow_links);
  if (variables_[variable_index].first == Program::VariableType::Int32) {
    appendToPrintBuffer(std::to_string(variables_[variable_index].second));
  } else if (variables_[variable_index].first == Program::VariableType::Link) {
    appendToPrintBuffer("L{" + std::to_string(variables_[variable_index].second) + "}");
  } else {
    throw std::runtime_error("Cannot print unsupported variable type (" +
                             std::to_string(static_cast<int32_t>(variables_[variable_index].first)) + ").");
  }
}

const std::string& VmSession::getPrintBuffer() const {
  return print_buffer_;
}

void VmSession::clearPrintBuffer() {
  print_buffer_ = "";
}

}  // namespace beast
