#include <beast/vm_session.hpp>

#include <random>
#include <set>
#include <stdexcept>

namespace beast {

VmSession::VmSession(
    Program program, size_t variable_count, size_t string_table_count,
    size_t max_string_size)
  : program_{std::move(program)}, pointer_{0}, variable_count_{variable_count}
  , string_table_count_{string_table_count}, max_string_size_{max_string_size}
  , was_terminated_{false}, return_code_{0} {
}

void VmSession::setVariableBehavior(int32_t variable_index, VariableIoBehavior behavior) {
  VariableDescriptor descriptor({Program::VariableType::Int32, behavior, false});
  variables_[variable_index] = std::make_pair(descriptor, 0);
}

VmSession::VariableIoBehavior VmSession::getVariableBehavior(
    int32_t variable_index, bool follow_links) {
  const auto iterator = variables_.find(getRealVariableIndex(variable_index, follow_links));
  if (iterator == variables_.end()) {
    throw std::runtime_error("Variable index not declared");
  }
  return iterator->second.first.behavior;
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

int32_t VmSession::getVariableValue(int32_t variable_index, bool follow_links) {
  std::pair<VariableDescriptor, int32_t>& variable = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.first.behavior == VariableIoBehavior::Output) {
    variable.first.changed_since_last_interaction = false;
  }
  return variable.second;
}

int32_t VmSession::getVariableValueInternal(int32_t variable_index, bool follow_links) {
  std::pair<VariableDescriptor, int32_t>& variable = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.first.behavior == VariableIoBehavior::Input) {
    variable.first.changed_since_last_interaction = false;
  }
  return variable.second;
}

void VmSession::setVariableValue(int32_t variable_index, bool follow_links, int32_t value) {
  std::pair<VariableDescriptor, int32_t>& variable = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.first.behavior == VariableIoBehavior::Input) {
    variable.first.changed_since_last_interaction = true;
  }
  variable.second = value;
}

void VmSession::setVariableValueInternal(int32_t variable_index, bool follow_links, int32_t value) {
  std::pair<VariableDescriptor, int32_t>& variable = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.first.behavior == VariableIoBehavior::Output) {
    variable.first.changed_since_last_interaction = true;
  }
  variable.second = value;
}

bool VmSession::isAtEnd() const {
  return was_terminated_ || pointer_ >= program_.getSize();
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

  VariableDescriptor descriptor({variable_type, VariableIoBehavior::Store, false});
  variables_[variable_index] = std::make_pair(descriptor, 0);
}

int32_t VmSession::getRealVariableIndex(int32_t variable_index, bool follow_links) {
  std::set<int32_t> visited_indices;
  std::map<int32_t, std::pair<VariableDescriptor, int32_t>>::iterator iterator;

  while ((iterator = variables_.find(variable_index)) != variables_.end()) {
    if (iterator->second.first.type != Program::VariableType::Link || !follow_links) {
      // This is a non-link variable, return it.
      return variable_index;
    }

    if (iterator->second.first.type == Program::VariableType::Link) {
      if (visited_indices.find(variable_index) != visited_indices.end()) {
        throw std::runtime_error("Circular variable index link.");
      }
      visited_indices.insert(variable_index);
      variable_index = getVariableValueInternal(variable_index, false);
    } else {
      throw std::runtime_error("Invalid declarative variable type.");
    }
  }

  // Undeclared variables cannot be set.
  throw std::runtime_error("Variable index not declared.");
}

void VmSession::setVariable(int32_t variable_index, int32_t value, bool follow_links) {
  setVariableValueInternal(variable_index, follow_links, value);
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

const std::string& VmSession::getStringTableEntry(int32_t string_table_index) const {
  const auto iterator = string_table_.find(string_table_index);
  if (iterator == string_table_.end()) {
    throw std::runtime_error("String table index out of bounds.");
  }

  return iterator->second;
}

void VmSession::appendToPrintBuffer(const std::string& string) {
  print_buffer_ += string;
}

void VmSession::appendVariableToPrintBuffer(
    int32_t variable_index, bool follow_links, bool as_char) {
  variable_index = getRealVariableIndex(variable_index, follow_links);
  if (variables_[variable_index].first.type == Program::VariableType::Int32) {
    if (as_char) {
      const uint32_t flag = 0xff;
      const auto val = static_cast<char>(static_cast<uint32_t>(getVariableValueInternal(variable_index, false)) & flag);
      appendToPrintBuffer(std::string(&val, 1));
    } else {
      appendToPrintBuffer(std::to_string(getVariableValueInternal(variable_index, false)));
    }
  } else if (variables_[variable_index].first.type == Program::VariableType::Link) {
    appendToPrintBuffer("L{" + std::to_string(getVariableValueInternal(variable_index, false)) + "}");
  } else {
    throw std::runtime_error("Cannot print unsupported variable type (" +
                             std::to_string(static_cast<int32_t>(variables_[variable_index].first.type)) + ").");
  }
}

const std::string& VmSession::getPrintBuffer() const {
  return print_buffer_;
}

void VmSession::clearPrintBuffer() {
  print_buffer_ = "";
}

void VmSession::terminate(int8_t return_code) {
  return_code_ = return_code;
  was_terminated_ = true;
}

int8_t VmSession::getReturnCode() const {
  return return_code_;
}

void VmSession::addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links) {
  setVariableValueInternal(variable_index, follow_links, getVariableValueInternal(variable_index, follow_links) + constant);
}

void VmSession::addVariableToVariable(
    int32_t source_variable, int32_t destination_variable, bool follow_source_links,
    bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      getVariableValueInternal(destination_variable, follow_destination_links) +
      getVariableValueInternal(source_variable, follow_source_links));
}

void VmSession::subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links) {
  setVariableValueInternal(variable_index, follow_links, getVariableValueInternal(variable_index, follow_links) - constant);
}

void VmSession::subtractVariableFromVariable(
    int32_t source_variable, int32_t destination_variable, bool follow_source_links,
    bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      getVariableValueInternal(destination_variable, follow_destination_links) -
      getVariableValueInternal(source_variable, follow_source_links));
}

void VmSession::relativeJumpToVariableAddressIfVariableGt0(
    int32_t condition_variable, bool follow_condition_links,
    int32_t addr_variable, bool follow_addr_links) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) > 0) {
    pointer_ += getVariableValueInternal(addr_variable, follow_addr_links);
  }
}

void VmSession::relativeJumpToVariableAddressIfVariableLt0(
    int32_t condition_variable, bool follow_condition_links,
    int32_t addr_variable, bool follow_addr_links) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) < 0) {
    pointer_ += getVariableValueInternal(addr_variable, follow_addr_links);
  }
}

void VmSession::relativeJumpToVariableAddressIfVariableEq0(
    int32_t condition_variable, bool follow_condition_links,
    int32_t addr_variable, bool follow_addr_links) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) == 0) {
    pointer_ += getVariableValueInternal(addr_variable, follow_addr_links);
  }
}

void VmSession::absoluteJumpToVariableAddressIfVariableGt0(
    int32_t condition_variable, bool follow_condition_links,
    int32_t addr_variable, bool follow_addr_links) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) > 0) {
    pointer_ = getVariableValueInternal(addr_variable, follow_addr_links);
  }
}

void VmSession::absoluteJumpToVariableAddressIfVariableLt0(
    int32_t condition_variable, bool follow_condition_links,
    int32_t addr_variable, bool follow_addr_links) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) < 0) {
    pointer_ = getVariableValueInternal(addr_variable, follow_addr_links);
  }
}

void VmSession::absoluteJumpToVariableAddressIfVariableEq0(
    int32_t condition_variable, bool follow_condition_links,
    int32_t addr_variable, bool follow_addr_links) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) == 0) {
    pointer_ = getVariableValueInternal(addr_variable, follow_addr_links);
  }
}

void VmSession::relativeJumpToAddressIfVariableGt0(
    int32_t condition_variable, bool follow_condition_links, int32_t addr) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) > 0) {
    pointer_ += addr;
  }
}

void VmSession::relativeJumpToAddressIfVariableLt0(
    int32_t condition_variable, bool follow_condition_links, int32_t addr) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) < 0) {
    pointer_ += addr;
  }
}

void VmSession::relativeJumpToAddressIfVariableEq0(
    int32_t condition_variable, bool follow_condition_links, int32_t addr) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) == 0) {
    pointer_ += addr;
  }
}

void VmSession::absoluteJumpToAddressIfVariableGt0(
    int32_t condition_variable, bool follow_condition_links, int32_t addr) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) > 0) {
    pointer_ = addr;
  }
}

void VmSession::absoluteJumpToAddressIfVariableLt0(
    int32_t condition_variable, bool follow_condition_links, int32_t addr) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) < 0) {
    pointer_ = addr;
  }
}

void VmSession::absoluteJumpToAddressIfVariableEq0(
    int32_t condition_variable, bool follow_condition_links, int32_t addr) {
  if (getVariableValueInternal(condition_variable, follow_condition_links) == 0) {
    pointer_ = addr;
  }
}

void VmSession::loadMemorySizeIntoVariable(int32_t variable, bool follow_links) {
  setVariableValueInternal(variable, follow_links, static_cast<int32_t>(variable_count_));
}

void VmSession::checkIfVariableIsInput(
    int32_t source_variable, bool follow_source_links,
    int32_t destination_variable, bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      variables_[getRealVariableIndex(source_variable, follow_source_links)].first.behavior == VmSession::VariableIoBehavior::Input ? 0x1 : 0x0);
}

void VmSession::checkIfVariableIsOutput(
    int32_t source_variable, bool follow_source_links,
    int32_t destination_variable, bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      variables_[getRealVariableIndex(source_variable, follow_source_links)].first.behavior == VmSession::VariableIoBehavior::Output ? 0x1 : 0x0);
}

void VmSession::copyVariable(
    int32_t source_variable, bool follow_source_links,
    int32_t destination_variable, bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      getVariableValueInternal(source_variable, follow_source_links));
}

void VmSession::loadInputCountIntoVariable(int32_t variable, bool follow_links) {
  int32_t input_count = 0;
  for (const std::pair<const int32_t, std::pair<VariableDescriptor, int32_t>>& pair : variables_) {
    if (pair.second.first.behavior == VariableIoBehavior::Input) {
      input_count++;
    }
  }
  setVariableValueInternal(variable, follow_links, input_count);
}

void VmSession::loadOutputCountIntoVariable(int32_t variable, bool follow_links) {
  int32_t output_count = 0;
  for (const std::pair<const int32_t, std::pair<VariableDescriptor, int32_t>>& pair : variables_) {
    if (pair.second.first.behavior == VariableIoBehavior::Output) {
      output_count++;
    }
  }
  setVariableValueInternal(variable, follow_links, output_count);
}

void VmSession::loadCurrentAddressIntoVariable(int32_t variable, bool follow_links) {
  setVariableValueInternal(variable, follow_links, pointer_);
}

void VmSession::checkIfInputWasSet(
    int32_t variable_index, bool follow_links,
    int32_t destination_variable, bool follow_destination_links) {
  std::pair<VariableDescriptor, int32_t>& variable = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.first.behavior != VariableIoBehavior::Input) {
    throw std::runtime_error("Variable is not an input.");
  }

  setVariableValueInternal(destination_variable, follow_destination_links, variable.first.changed_since_last_interaction ? 0x1 : 0x0);
  variable.first.changed_since_last_interaction = false;
}

void VmSession::loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links) {
  setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(string_table_count_));
}

void VmSession::loadStringTableItemLengthLimitIntoVariable(
    int32_t variable_index, bool follow_links) {
  setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(max_string_size_));
}

void VmSession::loadRandomValueIntoVariable(int32_t variable_index, bool follow_links) {
  // NOTE(fairlight1337): The initialization of the `rng` variable is not linted here to prevent
  // clang-tidy from complain about seeding with a value from the default constructor. Since we're
  // using `random_device` to seed `rng` right after, this warning is discarded explicitly.
  // NOLINTNEXTLINE
  std::mt19937 rng;
  std::random_device random_device;
  rng.seed(random_device());
  std::uniform_int_distribution<int32_t> distribution;
  setVariableValueInternal(variable_index, follow_links, distribution(rng));
}

void VmSession::unconditionalJumpToAbsoluteAddress(int32_t addr) {
  pointer_ = addr;
}

void VmSession::unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links) {
  pointer_ = getVariableValueInternal(variable_index, follow_links);
}

void VmSession::unconditionalJumpToRelativeAddress(int32_t addr) {
  pointer_ += addr;
}

void VmSession::unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links) {
  pointer_ += getVariableValueInternal(variable_index, follow_links);
}

void VmSession::loadStringItemLengthIntoVariable(
    int32_t string_table_index, int32_t variable_index, bool follow_links) {
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::runtime_error("String table index out of bounds.");
  }

  setVariableValueInternal(
      variable_index, follow_links, static_cast<int32_t>(string_table_[string_table_index].size()));
}

void VmSession::loadStringItemIntoVariables(
    int32_t string_table_index, int32_t start_variable_index, bool follow_links) {
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::runtime_error("String table index out of bounds.");
  }

  const auto iterator = string_table_.find(string_table_index);
  if (iterator == string_table_.end()) {
    throw std::runtime_error("String table index not defined.");
  }

  const size_t size = iterator->second.size();
  for (uint32_t idx = 0; idx < size; ++idx) {
    setVariableValueInternal(
        start_variable_index + static_cast<int32_t>(idx), follow_links,
        static_cast<int32_t>(iterator->second.at(idx)));
  }
}

void VmSession::bitShiftVariable(int32_t variable_index, bool follow_links, int8_t places) {
  const auto value = static_cast<uint32_t>(getVariableValueInternal(variable_index, follow_links));
  const auto shifted_value = places > 0 ? value << static_cast<uint32_t>(places) : value >> static_cast<uint32_t>(-places);
  setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(shifted_value));
}

}  // namespace beast
