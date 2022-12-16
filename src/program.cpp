#include <beast/program.hpp>

#include <cstring>
#include <stdexcept>

namespace beast {

Program::Program()
  : pointer_{0}, grows_dynamically_{true} {
}

Program::Program(int32_t space)
  : data_(space, 0x00), pointer_{0}, grows_dynamically_{false} {
}

int32_t Program::getSize() const {
  return data_.size();
}

int32_t Program::getData4(int32_t offset) {
  if ((offset + 4) > getSize()) {
    throw std::runtime_error("Unable to retrieve data (not enough data left).");
  }

  int32_t buffer = 0x0;
  std::memcpy(&buffer, &data_[offset], 4);
  return buffer;
}

int16_t Program::getData2(int32_t offset) {
  if ((offset + 2) > getSize()) {
    throw std::runtime_error("Unable to retrieve data (not enough data left).");
  }

  int16_t buffer = 0x0;
  std::memcpy(&buffer, &data_[offset], 2);
  return buffer;
}

int8_t Program::getData1(int32_t offset) {
  if ((offset + 1) > getSize()) {
    throw std::runtime_error("Unable to retrieve data (not enough data left).");
  }

  int8_t buffer = 0x0;
  std::memcpy(&buffer, &data_[offset], 1);
  return buffer;
}

int32_t Program::getPointer() const {
  return pointer_;
}

void Program::insertProgram(Program& other) {
  const int32_t to_fit = other.getSize();
  if (!canFit(to_fit)) { 
    throw std::runtime_error("Unable to fit other program into program.");
  }
  for (int32_t idx = 0; idx < to_fit; ++idx) {
    data_[pointer_ + idx] = other.getData()[idx];
  }
  pointer_ += to_fit;
}

const std::vector<unsigned char>& Program::getData() const {
  return data_;
}

void Program::noop() {
  appendCode1(OpCode::NoOp);
}

void Program::declareVariable(int32_t variable_index, VariableType variable_type) {
  appendCode1(OpCode::DeclareVariable);
  appendData4(variable_index);
  appendData1(static_cast<unsigned char>(variable_type));
}

void Program::setVariable(int32_t variable_index, int32_t content, bool follow_links) {
  appendCode1(OpCode::SetVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(content);
}

void Program::undeclareVariable(int32_t variable_index) {
  appendCode1(OpCode::UndeclareVariable);
  appendData4(variable_index);
}

void Program::addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links) {
  appendCode1(OpCode::AddConstantToVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
}

void Program::addVariableToVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::AddVariableToVariable);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links) {
  appendCode1(OpCode::SubtractConstantFromVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
}

void Program::subtractVariableFromVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::SubtractVariableFromVariable);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links) {
  appendCode1(OpCode::RelativeJumpToVariableAddressIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links) {
  appendCode1(OpCode::RelativeJumpToVariableAddressIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links) {
  appendCode1(OpCode::RelativeJumpToVariableAddressIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links) {
  appendCode1(OpCode::AbsoluteJumpToVariableAddressIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links) {
  appendCode1(OpCode::AbsoluteJumpToVariableAddressIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links) {
  appendCode1(OpCode::AbsoluteJumpToVariableAddressIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address) {
  appendCode1(OpCode::RelativeJumpIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address);
}

void Program::relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address) {
  appendCode1(OpCode::RelativeJumpIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address);
}

void Program::relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address) {
  appendCode1(OpCode::RelativeJumpIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address);
}

void Program::absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address) {
  appendCode1(OpCode::AbsoluteJumpIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address);
}

void Program::absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address) {
  appendCode1(OpCode::AbsoluteJumpIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address);
}

void Program::absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address) {
  appendCode1(OpCode::AbsoluteJumpIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address);
}

void Program::loadMemorySizeIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadMemorySizeIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::checkIfVariableIsInput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::CheckIfVariableIsInput);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::checkIfVariableIsOutput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::CheckIfVariableIsOutput);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::loadInputCountIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadInputCountIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadOutputCountIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadOutputCountIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadCurrentAddressIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadCurrentAddressIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::printVariable(int32_t variable_index, bool follow_links, bool as_char) {
  appendCode1(OpCode::PrintVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendFlag1(as_char);
}

void Program::setStringTableEntry(int32_t string_table_index, std::string string) {
  if (!canFit(3 + string.size())) {
    throw std::runtime_error("Unable to fit instruction into program.");
  }

  appendCode1(OpCode::SetStringTableEntry);
  appendData4(string_table_index);
  appendData2(string.size());
  std::memcpy(&data_[pointer_], string.data(), string.size());
  pointer_ += string.size();
}

void Program::printStringFromStringTable(int32_t string_table_index) {
  appendCode1(OpCode::PrintStringFromStringTable);
  appendData4(string_table_index);
}

void Program::loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadStringTableLimitIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::terminate(int8_t return_code) {
  appendCode1(OpCode::Terminate);
  appendData1(return_code);
}

void Program::copyVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::CopyVariable);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::checkIfInputWasSet(
    int32_t variable_index, bool follow_links,
    int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::CheckIfInputWasSet);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::loadStringTableItemLengthLimitIntoVariable(
    int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadStringTableItemLengthLimitIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadRandomValueIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadRandomValueIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

bool Program::canFit(int32_t bytes) {
  if (grows_dynamically_) {
    ensureSize(pointer_ + bytes);
  }
  return bytes <= data_.size() - pointer_;
}

void Program::appendData4(int32_t data) {
  if (!canFit(4)) { throw std::runtime_error("Unable to fit data into program."); }

  std::memcpy(&data_[pointer_], &data, 4);
  pointer_ += 4;
}

void Program::appendData2(int16_t data) {
  if (!canFit(2)) { throw std::runtime_error("Unable to fit data into program."); }

  std::memcpy(&data_[pointer_], &data, 2);
  pointer_ += 2;
}

void Program::appendData1(int8_t data) {
  if (!canFit(1)) { throw std::runtime_error("Unable to fit data into program."); }

  std::memcpy(&data_[pointer_], &data, 1);
  pointer_ += 1;
}

void Program::appendFlag1(bool flag) {
  appendData1(flag ? 0x1 : 0x0);
}

void Program::appendCode1(OpCode opcode) {
  appendData1(static_cast<int8_t>(opcode));
}

void Program::ensureSize(int32_t size) {
  if (data_.size() < size) {
    data_.resize(size);
  }
}

}  // namespace beast
