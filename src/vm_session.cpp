#include <beast/vm_session.hpp>

// Standard
#include <chrono>
#include <ctime>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>

// Internal
#include <beast/time_functions.hpp>

namespace beast {

VmSession::VmSession(
    Program program, size_t variable_count, size_t string_table_count,
    size_t max_string_size)
  : program_{std::move(program)}, variable_count_{variable_count}
  , string_table_count_{string_table_count}, max_string_size_{max_string_size} {
  resetRuntimeStatistics();
}

void VmSession::informAboutStep(OpCode operator_code) noexcept {
  runtime_statistics_.steps_executed++;
  runtime_statistics_.operator_executions[operator_code]++;
  runtime_statistics_.executed_indices.insert(pointer_);
}

void VmSession::resetRuntimeStatistics() noexcept {
  runtime_statistics_ = RuntimeStatistics{};
}

void VmSession::reset() noexcept {
  resetRuntimeStatistics();
  variables_ = std::map<int32_t, std::pair<VariableDescriptor, int32_t>>{};
  string_table_ = std::map<int32_t, std::string>{};
  print_buffer_ = "";
  pointer_ = 0;
}

const VmSession::RuntimeStatistics& VmSession::getRuntimeStatistics() const noexcept {
  return runtime_statistics_;
}

void VmSession::setVariableBehavior(int32_t variable_index, VariableIoBehavior behavior) {
  VariableDescriptor descriptor({Program::VariableType::Int32, behavior, false});
  variables_[variable_index] = std::make_pair(descriptor, 0);
}

VmSession::VariableIoBehavior VmSession::getVariableBehavior(
    int32_t variable_index, bool follow_links) {
  const auto iterator = variables_.find(getRealVariableIndex(variable_index, follow_links));
  if (iterator == variables_.end()) {
    throw std::invalid_argument("Variable index not declared");
  }
  return iterator->second.first.behavior;
}

bool VmSession::hasOutputDataAvailable(int32_t variable_index, bool follow_links) {
  const auto iterator = variables_.find(getRealVariableIndex(variable_index, follow_links));
  if (iterator == variables_.end()) {
    throw std::invalid_argument("Variable index not declared");
  }
  if (iterator->second.first.behavior != VariableIoBehavior::Output) {
    throw std::invalid_argument("Variable behavior not declared as output");
  }
  return iterator->second.first.changed_since_last_interaction;
}

void VmSession::setMaximumPrintBufferLength(size_t maximum_print_buffer_length) {
  maximum_print_buffer_length_ = maximum_print_buffer_length;
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
  auto& [variable, value] = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.behavior == VariableIoBehavior::Output) {
    variable.changed_since_last_interaction = false;
  }
  return value;
}

int32_t VmSession::getVariableValueInternal(int32_t variable_index, bool follow_links) {
  auto& [variable, value] = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.behavior == VariableIoBehavior::Input) {
    variable.changed_since_last_interaction = false;
  }
  return value;
}

void VmSession::setVariableValue(int32_t variable_index, bool follow_links, int32_t value) {
  auto& [variable, current_value] = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.behavior == VariableIoBehavior::Input) {
    variable.changed_since_last_interaction = true;
  }
  current_value = value;
}

void VmSession::setVariableValueInternal(int32_t variable_index, bool follow_links, int32_t value) {
  auto& [variable, current_value] = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.behavior == VariableIoBehavior::Output) {
    variable.changed_since_last_interaction = true;
  }
  current_value = value;
}

bool VmSession::isAtEnd() const noexcept {
  return runtime_statistics_.terminated || pointer_ >= program_.getSize();
}

void VmSession::setExitedAbnormally() {
  runtime_statistics_.abnormal_exit = true;
}

void VmSession::registerVariable(int32_t variable_index, Program::VariableType variable_type) {
  if (variable_index < 0 || variable_index >= variable_count_) {
    throw std::out_of_range("Invalid variable index.");
  }

  if (variable_type != Program::VariableType::Int32 &&
      variable_type != Program::VariableType::Link) {
    throw std::invalid_argument("Invalid declarative variable type.");
  }

  if (variables_.find(variable_index) != variables_.end()) {
    throw std::invalid_argument("Variable index already declared.");
  }

  if (variables_.size() == variable_count_) {
    // No more space for variables
    throw std::overflow_error("Variables cache full.");
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
        throw std::invalid_argument("Circular variable index link.");
      }
      visited_indices.insert(variable_index);
      variable_index = getVariableValueInternal(variable_index, false);
    } else {
      throw std::invalid_argument("Invalid declarative variable type.");
    }
  }

  // Undeclared variables cannot be set.
  throw std::invalid_argument("Variable index not declared.");
}

void VmSession::setVariable(int32_t variable_index, int32_t value, bool follow_links) {
  setVariableValueInternal(variable_index, follow_links, value);
}

void VmSession::unregisterVariable(int32_t variable_index) {
  if (variable_index < 0 || variable_index >= variable_count_) {
    throw std::out_of_range("Invalid variable index.");
  }

  if (variables_.find(variable_index) == variables_.end()) {
    throw std::invalid_argument("Variable index not declared, cannot undeclare.");
  }

  variables_.erase(variable_index);
}

void VmSession::setStringTableEntry(int32_t string_table_index, std::string_view string_content) {
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  if (string_content.size() > max_string_size_) {
    throw std::length_error("String too long.");
  }

  string_table_[string_table_index] = string_content;
}

const std::string& VmSession::getStringTableEntry(int32_t string_table_index) const {
  const auto iterator = string_table_.find(string_table_index);
  if (iterator == string_table_.end()) {
    throw std::out_of_range("String table index out of bounds.");
  }

  return iterator->second;
}

void VmSession::appendToPrintBuffer(std::string_view string) {
  if (print_buffer_.size() + string.size() > maximum_print_buffer_length_) {
    throw std::overflow_error("Print buffer overflow.");
  }
  print_buffer_ += string;
}

void VmSession::appendVariableToPrintBuffer(
    int32_t variable_index, bool follow_links, bool as_char) {
  variable_index = getRealVariableIndex(variable_index, follow_links);
  if (variables_[variable_index].first.type == Program::VariableType::Int32) {
    if (as_char) {
      const uint32_t flag = 0xff;
      const auto val =
          static_cast<char>(
              static_cast<uint32_t>(getVariableValueInternal(variable_index, false)) & flag);
      appendToPrintBuffer(std::string_view(&val, 1));
    } else {
      appendToPrintBuffer(std::to_string(getVariableValueInternal(variable_index, false)));
    }
  } else if (variables_[variable_index].first.type == Program::VariableType::Link) {
    appendToPrintBuffer(
        "L{" + std::to_string(getVariableValueInternal(variable_index, false)) + "}");
  } else {
    throw std::invalid_argument("Cannot print unsupported variable type (" +
                                std::to_string(
                                    static_cast<int32_t>(variables_[variable_index].first.type))
                                + ").");
  }
}

const std::string& VmSession::getPrintBuffer() const {
  return print_buffer_;
}

void VmSession::clearPrintBuffer() {
  print_buffer_ = "";
}

void VmSession::terminate(int8_t return_code) {
  runtime_statistics_.return_code = return_code;
  runtime_statistics_.terminated = true;
}

void VmSession::addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links) {
  setVariableValueInternal(
      variable_index, follow_links,
      getVariableValueInternal(variable_index, follow_links) + constant);
}

void VmSession::addVariableToVariable(
    int32_t source_variable, int32_t destination_variable, bool follow_source_links,
    bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      getVariableValueInternal(destination_variable, follow_destination_links) +
      getVariableValueInternal(source_variable, follow_source_links));
}

void VmSession::subtractConstantFromVariable(
    int32_t variable_index, int32_t constant, bool follow_links) {
  setVariableValueInternal(
      variable_index, follow_links,
      getVariableValueInternal(variable_index, follow_links) - constant);
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
      variables_[getRealVariableIndex(source_variable, follow_source_links)].first.behavior
      == VmSession::VariableIoBehavior::Input ? 0x1 : 0x0);
}

void VmSession::checkIfVariableIsOutput(
    int32_t source_variable, bool follow_source_links,
    int32_t destination_variable, bool follow_destination_links) {
  setVariableValueInternal(
      destination_variable, follow_destination_links,
      variables_[getRealVariableIndex(source_variable, follow_source_links)].first.behavior
      == VmSession::VariableIoBehavior::Output ? 0x1 : 0x0);
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
  for (const auto& [index, variable_data] : variables_) {
    if (variable_data.first.behavior == VariableIoBehavior::Input) {
      input_count++;
    }
  }
  setVariableValueInternal(variable, follow_links, input_count);
}

void VmSession::loadOutputCountIntoVariable(int32_t variable, bool follow_links) {
  int32_t output_count = 0;
  for (const auto& [index, variable_data] : variables_) {
    if (variable_data.first.behavior == VariableIoBehavior::Output) {
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
  auto& [variable, value] = variables_[getRealVariableIndex(variable_index, follow_links)];
  if (variable.behavior != VariableIoBehavior::Input) {
    throw std::invalid_argument("Variable is not an input.");
  }

  setVariableValueInternal(
      destination_variable, follow_destination_links,
      variable.changed_since_last_interaction ? 0x1 : 0x0);
  variable.changed_since_last_interaction = false;
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
  // clang-tidy from complaining about seeding with a value from the default constructor. Since
  // we're using `random_device` to seed `rng` right after, this warning is discarded explicitly.
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

void VmSession::unconditionalJumpToAbsoluteVariableAddress(
    int32_t variable_index, bool follow_links) {
  pointer_ = getVariableValueInternal(variable_index, follow_links);
}

void VmSession::unconditionalJumpToRelativeAddress(int32_t addr) {
  pointer_ += addr;
}

void VmSession::unconditionalJumpToRelativeVariableAddress(
    int32_t variable_index, bool follow_links) {
  pointer_ += getVariableValueInternal(variable_index, follow_links);
}

void VmSession::loadStringItemLengthIntoVariable(
    int32_t string_table_index, int32_t variable_index, bool follow_links) {
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  setVariableValueInternal(
      variable_index, follow_links, static_cast<int32_t>(string_table_[string_table_index].size()));
}

void VmSession::loadStringItemIntoVariables(
    int32_t string_table_index, int32_t start_variable_index, bool follow_links) {
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  const auto iterator = string_table_.find(string_table_index);
  if (iterator == string_table_.end()) {
    throw std::invalid_argument("String table index not defined.");
  }

  const size_t size = iterator->second.size();
  for (uint32_t idx = 0; idx < size; ++idx) {
    setVariableValueInternal(
        start_variable_index + static_cast<int32_t>(idx), follow_links,
        static_cast<int32_t>(iterator->second.at(idx)));
  }
}

void VmSession::performSystemCall(
    int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links) {
  if (major_code == 0) {  // Time and date related functions
    // Get the current UTC time
    const auto now_utc = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc_tm{};
    gmtime_r(&now_utc, &utc_tm);

    // Get the current local time
    const auto now_local = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local_tm{};
    localtime_r(&now_local, &local_tm);

    const int32_t offset_minutes =
        (local_tm.tm_hour - utc_tm.tm_hour) * 60 + (local_tm.tm_min - utc_tm.tm_min);

    switch (minor_code) {
    case 0: {  // UTC Timezone (hours)
      // Calculate the timezone offset in minutes
      const auto utc_offset_hours =
          static_cast<int32_t>(std::floor(static_cast<double>(offset_minutes) / 60.0));
      setVariableValueInternal(variable_index, follow_links, utc_offset_hours);
    } break;

    case 1: {  // UTC Timezone (minutes)
      // Calculate the timezone offset in minutes
      const int32_t utc_offset_minutes = offset_minutes % 60;
      setVariableValueInternal(variable_index, follow_links, utc_offset_minutes);
    } break;

    case 2: {  // Seconds
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_sec);
    } break;

    case 3: {  // Minutes
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_min);
    } break;

    case 4: {  // Hours
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_hour);
    } break;

    case 5: {  // Day
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_mday);
    } break;

    case 6: {  // Month
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_mon);
    } break;

    case 7: {  // Year
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_year);
    } break;

    case 8: {  // Week
      const int32_t current_year = utc_tm.tm_year + 1900;
      const int32_t current_day = utc_tm.tm_mday;

      // Calculate the first day of the current year
      std::tm first_day_of_year{};
      first_day_of_year.tm_year = current_year - 1900;
      first_day_of_year.tm_mon = 0;  // January
      first_day_of_year.tm_mday = 1;

      const std::time_t first_day_time = std::mktime(&first_day_of_year);
      std::tm first_day_tm{};
      gmtime_r(&first_day_time, &first_day_tm);
      const int32_t first_day_weekday = first_day_tm.tm_wday;

      const int32_t current_week = (current_day - 1 + first_day_weekday) / 7 + 1;
      setVariableValueInternal(variable_index, follow_links, current_week);
    } break;

    case 9: {  // Day of Week
      setVariableValueInternal(variable_index, follow_links, utc_tm.tm_wday);
    } break;

    default:
      throw std::invalid_argument("Unknown major/minor code combination for system call: "
                                  + std::to_string(major_code) + ", " + std::to_string(minor_code));
    }
  } else {
    throw std::invalid_argument("Unknown major code for system call: "
                                + std::to_string(major_code));
  }
}

void VmSession::bitShiftVariable(int32_t variable_index, bool follow_links, int8_t places) {
  const auto value = static_cast<uint32_t>(getVariableValueInternal(variable_index, follow_links));
  const auto shifted_value =
      places > 0 ? value << static_cast<uint32_t>(places) : value >> static_cast<uint32_t>(-places);
  setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(shifted_value));
}

void VmSession::bitWiseInvertVariable(int32_t variable_index, bool follow_links) {
  const auto value = static_cast<uint32_t>(getVariableValueInternal(variable_index, follow_links));
  const auto inverted_value = ~value;
  setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(inverted_value));
}

void VmSession::bitWiseAndTwoVariables(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b) {
  const auto value_a =
      static_cast<uint32_t>(getVariableValueInternal(variable_index_a, follow_links_a));
  const auto value_b =
      static_cast<uint32_t>(getVariableValueInternal(variable_index_b, follow_links_b));
  const uint32_t result = value_a & value_b;
  setVariableValueInternal(variable_index_b, follow_links_b, static_cast<int32_t>(result));
}

void VmSession::bitWiseOrTwoVariables(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b) {
  const auto value_a =
      static_cast<uint32_t>(getVariableValueInternal(variable_index_a, follow_links_a));
  const auto value_b =
      static_cast<uint32_t>(getVariableValueInternal(variable_index_b, follow_links_b));
  const uint32_t result = value_a | value_b;
  setVariableValueInternal(variable_index_b, follow_links_b, static_cast<int32_t>(result));
}

void VmSession::bitWiseXorTwoVariables(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b) {
  const auto value_a =
      static_cast<uint32_t>(getVariableValueInternal(variable_index_a, follow_links_a));
  const auto value_b =
      static_cast<uint32_t>(getVariableValueInternal(variable_index_b, follow_links_b));
  const uint32_t result = value_a ^ value_b;
  setVariableValueInternal(variable_index_b, follow_links_b, static_cast<int32_t>(result));
}

void VmSession::moduloVariableByConstant(
    int32_t variable_index, bool follow_links, int32_t constant) {
  if (constant <= 0) {
    throw std::invalid_argument("Cannot modulo with a constant <= 0.");
  }

  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  const int32_t result = value % constant;
  setVariableValueInternal(variable_index, follow_links, result);
}

void VmSession::moduloVariableByVariable(
    int32_t variable_index, bool follow_links,
    int32_t modulo_variable_index, bool modulo_follow_links) {
  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  const int32_t modulo_value = getVariableValueInternal(modulo_variable_index, modulo_follow_links);
  if (modulo_value <= 0) {
    throw std::invalid_argument("Cannot modulo with a modulo value <= 0.");
  }

  const int32_t result = value % modulo_value;
  setVariableValueInternal(variable_index, follow_links, result);
}

void VmSession::rotateVariable(int32_t variable_index, bool follow_links, int8_t places) {
  const auto value = static_cast<uint32_t>(getVariableValueInternal(variable_index, follow_links));
  const auto abs_places = static_cast<uint8_t>(std::abs(places));
  if (places < 0) {
   // Rotate right
    const uint32_t result =
        (value >> abs_places) | (value << static_cast<uint32_t>(32 - abs_places));
    setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(result));
  } else {
    // Rotate left
    const uint32_t result =
        (value << abs_places) | (value >> static_cast<uint32_t>(32 - abs_places));
    setVariableValueInternal(variable_index, follow_links, static_cast<int32_t>(result));
  }
}

void VmSession::pushVariableOnStack(
    int32_t stack_variable_index, bool stack_follow_links,
    int32_t variable_index, bool follow_links) {
  const int32_t current_stack_size =
      getVariableValueInternal(stack_variable_index, stack_follow_links);
  const int32_t new_value = getVariableValueInternal(variable_index, follow_links);
  setVariableValueInternal(
    stack_variable_index + 1 + current_stack_size, stack_follow_links, new_value);
  setVariableValueInternal(stack_variable_index, stack_follow_links, current_stack_size + 1);
}

void VmSession::pushConstantOnStack(
    int32_t stack_variable_index, bool stack_follow_links, int32_t constant) {
  const int32_t current_stack_size =
      getVariableValueInternal(stack_variable_index, stack_follow_links);
  setVariableValueInternal(
      stack_variable_index + 1 + current_stack_size, stack_follow_links, constant);
  setVariableValueInternal(stack_variable_index, stack_follow_links, current_stack_size + 1);
}

void VmSession::popVariableFromStack(
    int32_t stack_variable_index, bool stack_follow_links,
    int32_t variable_index, bool follow_links) {
  const int32_t current_stack_size =
      getVariableValueInternal(stack_variable_index, stack_follow_links);
  if (current_stack_size == 0) {
    throw std::underflow_error("Cannot pop value from stack, stack empty.");
  }
  const int32_t last_value =
      getVariableValueInternal(
          stack_variable_index + 1 + current_stack_size - 1, stack_follow_links);
  setVariableValueInternal(stack_variable_index, true, current_stack_size - 1);
  setVariableValueInternal(variable_index, follow_links, last_value);
}

void VmSession::popTopItemFromStack(int32_t stack_variable_index, bool stack_follow_links) {
  const int32_t current_stack_size =
      getVariableValueInternal(stack_variable_index, stack_follow_links);
  if (current_stack_size == 0) {
    throw std::underflow_error("Cannot pop value from stack, stack empty.");
  }
  setVariableValueInternal(stack_variable_index, true, current_stack_size - 1);
}

void VmSession::checkIfStackIsEmpty(
    int32_t stack_variable_index, bool stack_follow_links,
    int32_t variable_index, bool follow_links) {
  const int32_t value = getVariableValueInternal(stack_variable_index, stack_follow_links);
  setVariableValueInternal(variable_index, follow_links, value == 0x0 ? 0x1 : 0x0);
}

void VmSession::swapVariables(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b) {
  const int32_t temp = getVariableValueInternal(variable_index_a, follow_links_a);
  setVariableValueInternal(
      variable_index_a, follow_links_a, getVariableValueInternal(variable_index_b, follow_links_b));
  setVariableValueInternal(variable_index_b, follow_links_b, temp);
}

void VmSession::setVariableStringTableEntry(
    int32_t variable_index, bool follow_links, std::string_view string_content) {
  const int32_t string_table_index = getVariableValueInternal(variable_index, follow_links);
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  if (string_content.size() > max_string_size_) {
    throw std::length_error("String too long.");
  }

  string_table_[string_table_index] = string_content;
}

void VmSession::printVariableStringFromStringTable(int32_t variable_index, bool follow_links) {
  const int32_t string_table_index = getVariableValueInternal(variable_index, follow_links);
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  appendToPrintBuffer(getStringTableEntry(string_table_index));
}

void VmSession::loadVariableStringItemLengthIntoVariable(
    int32_t string_item_variable_index, bool string_item_follow_links,
    int32_t variable_index, bool follow_links) {
  const int32_t string_table_index =
      getVariableValueInternal(string_item_variable_index, string_item_follow_links);
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  setVariableValueInternal(
      variable_index, follow_links, static_cast<int32_t>(string_table_[string_table_index].size()));
}

void VmSession::loadVariableStringItemIntoVariables(
    int32_t string_item_variable_index, bool string_item_follow_links,
    int32_t start_variable_index, bool follow_links) {
  const int32_t string_table_index =
      getVariableValueInternal(string_item_variable_index, string_item_follow_links);
  if (string_table_index < 0 || string_table_index >= string_table_count_) {
    throw std::out_of_range("String table index out of bounds.");
  }

  const auto iterator = string_table_.find(string_table_index);
  if (iterator == string_table_.end()) {
    throw std::invalid_argument("String table index not defined.");
  }

  const size_t size = iterator->second.size();
  for (uint32_t idx = 0; idx < size; ++idx) {
    setVariableValueInternal(
        start_variable_index + static_cast<int32_t>(idx), follow_links,
        static_cast<int32_t>(iterator->second.at(idx)));
  }
}

void VmSession::terminateWithVariableReturnCode(int32_t variable_index, bool follow_links) {
  const auto return_code =
      static_cast<int8_t>(getVariableValueInternal(variable_index, follow_links));
  runtime_statistics_.return_code = return_code;
  runtime_statistics_.terminated = true;
}

void VmSession::variableBitShiftVariableLeft(
    int32_t variable_index, bool follow_links,
    int32_t places_variable_index, bool places_follow_links) {
  const auto places =
      static_cast<int8_t>(getVariableValueInternal(places_variable_index, places_follow_links));
  bitShiftVariable(variable_index, follow_links, places);
}

void VmSession::variableBitShiftVariableRight(
    int32_t variable_index, bool follow_links,
    int32_t places_variable_index, bool places_follow_links) {
  const auto places =
      static_cast<int8_t>(getVariableValueInternal(places_variable_index, places_follow_links));
  bitShiftVariable(variable_index, follow_links, static_cast<int8_t>(-places));
}

void VmSession::variableRotateVariableLeft(
    int32_t variable_index, bool follow_links,
    int32_t places_variable_index, bool places_follow_links) {
  const auto places =
      static_cast<int8_t>(getVariableValueInternal(places_variable_index, places_follow_links));
  rotateVariable(variable_index, follow_links, places);
}

void VmSession::variableRotateVariableRight(
    int32_t variable_index, bool follow_links,
    int32_t places_variable_index, bool places_follow_links) {
  const auto places =
      static_cast<int8_t>(getVariableValueInternal(places_variable_index, places_follow_links));
  rotateVariable(variable_index, follow_links, static_cast<int8_t>(-places));
}

void VmSession::compareIfVariableGtConstant(
    int32_t variable_index, bool follow_links, int32_t constant,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value > constant ? 0x1 : 0x0);
}

void VmSession::compareIfVariableLtConstant(
    int32_t variable_index, bool follow_links, int32_t constant,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value < constant ? 0x1 : 0x0);
}

void VmSession::compareIfVariableEqConstant(
    int32_t variable_index, bool follow_links, int32_t constant,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value == constant ? 0x1 : 0x0);
}

void VmSession::compareIfVariableGtVariable(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value_a = getVariableValueInternal(variable_index_a, follow_links_a);
  const int32_t value_b = getVariableValueInternal(variable_index_b, follow_links_b);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value_a > value_b ? 0x1 : 0x0);
}

void VmSession::compareIfVariableLtVariable(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value_a = getVariableValueInternal(variable_index_a, follow_links_a);
  const int32_t value_b = getVariableValueInternal(variable_index_b, follow_links_b);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value_a < value_b ? 0x1 : 0x0);
}

void VmSession::compareIfVariableEqVariable(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value_a = getVariableValueInternal(variable_index_a, follow_links_a);
  const int32_t value_b = getVariableValueInternal(variable_index_b, follow_links_b);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value_a == value_b ? 0x1 : 0x0);
}

void VmSession::getMaxOfVariableAndConstant(
    int32_t variable_index, bool follow_links, int32_t constant,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value > constant ? value : constant);
}

void VmSession::getMinOfVariableAndConstant(
    int32_t variable_index, bool follow_links, int32_t constant,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value = getVariableValueInternal(variable_index, follow_links);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value < constant ? value : constant);
}

void VmSession::getMaxOfVariableAndVariable(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value_a = getVariableValueInternal(variable_index_a, follow_links_a);
  const int32_t value_b = getVariableValueInternal(variable_index_b, follow_links_b);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value_a > value_b ? value_a : value_b);
}

void VmSession::getMinOfVariableAndVariable(
    int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
    int32_t target_variable_index, bool target_follow_links) {
  const int32_t value_a = getVariableValueInternal(variable_index_a, follow_links_a);
  const int32_t value_b = getVariableValueInternal(variable_index_b, follow_links_b);
  setVariableValueInternal(
      target_variable_index, target_follow_links, value_a < value_b ? value_a : value_b);
}

void VmSession::printVariable(int32_t variable_index, bool follow_links, bool as_char) {
  appendVariableToPrintBuffer(variable_index, follow_links, as_char);
}

void VmSession::printStringFromStringTable(int32_t string_table_index) {
  appendToPrintBuffer(getStringTableEntry(string_table_index));
}

}  // namespace beast
