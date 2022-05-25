#include <beast/program.hpp>

#include <cstring>
#include <stdexcept>

namespace beast {

Program::Program(int32_t space)
  : data_(space, 0x00), pointer_{0} {
}

int32_t Program::getSize() {
  return data_.size();
}

int32_t Program::getData4(int32_t offset) {
  if ((offset + 4) > getSize()) {
    throw std::runtime_error("Unable to retrieve data (not enough data left).");
  }

  int32_t buffer;
  std::memcpy(&buffer, &data_[offset], 4);
  return buffer;
}

int16_t Program::getData2(int32_t offset) {
  if ((offset + 2) > getSize()) {
    throw std::runtime_error("Unable to retrieve data (not enough data left).");
  }

  int16_t buffer;
  std::memcpy(&buffer, &data_[offset], 2);
  return buffer;
}

int8_t Program::getData1(int32_t offset) {
  if ((offset + 1) > getSize()) {
    throw std::runtime_error("Unable to retrieve data (not enough data left).");
  }

  int8_t buffer;
  std::memcpy(&buffer, &data_[offset], 1);
  return buffer;
}

void Program::declareVariable(int32_t variable_index, VariableType variable_type) {
  appendData1(0x01);
  appendData4(variable_index);
  appendData1(static_cast<unsigned char>(variable_type));
}

void Program::setVariable(int32_t variable_index, int32_t content) {
  appendData1(0x02);
  appendData4(variable_index);
  appendData4(content);
}

void Program::undeclareVariable(int32_t variable_index) {
  appendData1(0x03);
  appendData4(variable_index);
}

void Program::addConstantToVariable(
    int32_t source_variable_index, int32_t destination_variable_index, int32_t constant_to_add) {
  appendData1(0x04);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
  appendData4(constant_to_add);
}

void Program::addVariableToVariable(
    int32_t source_variable_index, int32_t destination_variable_index,
    int32_t variable_index_to_add) {
  appendData1(0x05);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
  appendData4(variable_index_to_add);
}

void Program::subtractConstantFromVariable(
    int32_t source_variable_index, int32_t destination_variable_index, int32_t constant_to_subtract) {
  appendData1(0x06);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
  appendData4(constant_to_subtract);
}

void Program::subtractVariableFromVariable(
    int32_t source_variable_index, int32_t destination_variable_index,
    int32_t variable_index_to_subtract) {
  appendData1(0x07);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
  appendData4(variable_index_to_subtract);
}

void Program::relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t relative_jump_address_variable_index) {
  appendData1(0x08);
  appendData4(variable_index);
  appendData4(relative_jump_address_variable_index);
}

void Program::relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t relative_jump_address_variable_index) {
  appendData1(0x09);
  appendData4(variable_index);
  appendData4(relative_jump_address_variable_index);
}
                                                              
void Program::relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t relative_jump_address_variable_index) {
  appendData1(0x0a);
  appendData4(variable_index);
  appendData4(relative_jump_address_variable_index);
}

void Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t absolute_jump_address_variable_index) {
  appendData1(0x0b);
  appendData4(variable_index);
  appendData4(absolute_jump_address_variable_index);
}

void Program::absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t absolute_jump_address_variable_index) {
  appendData1(0x0c);
  appendData4(variable_index);
  appendData4(absolute_jump_address_variable_index);
}
                                                              
void Program::absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t absolute_jump_address_variable_index) {
  appendData1(0x0d);
  appendData4(variable_index);
  appendData4(absolute_jump_address_variable_index);
}

void Program::relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t relative_jump_address) {
  appendData1(0x0e);
  appendData4(variable_index);
  appendData4(relative_jump_address);
}

void Program::relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t relative_jump_address) {
  appendData1(0x0f);
  appendData4(variable_index);
  appendData4(relative_jump_address);
}
                                                              
void Program::relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t relative_jump_address) {
  appendData1(0x10);
  appendData4(variable_index);
  appendData4(relative_jump_address);
}

void Program::absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, int32_t absolute_jump_address) {
  appendData1(0x11);
  appendData4(variable_index);
  appendData4(absolute_jump_address);
}

void Program::absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, int32_t absolute_jump_address) {
  appendData1(0x12);
  appendData4(variable_index);
  appendData4(absolute_jump_address);
}
                                                              
void Program::absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, int32_t absolute_jump_address) {
  appendData1(0x13);
  appendData4(variable_index);
  appendData4(absolute_jump_address);
}

void Program::loadMemorySizeIntoVariable(int32_t variable_index) {
  appendData1(0x14);
  appendData4(variable_index);
}

void Program::checkIfVariableIsInput(
    int32_t source_variable_index, int32_t destination_variable_index) {
  appendData1(0x15);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
}

void Program::checkIfVariableIsOutput(
    int32_t source_variable_index, int32_t destination_variable_index) {
  appendData1(0x16);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
}

void Program::loadInputCountIntoVariable(int32_t variable_index) {
  appendData1(0x17);
  appendData4(variable_index);
}

void Program::loadOutputCountIntoVariable(int32_t variable_index) {
  appendData1(0x18);
  appendData4(variable_index);
}

void Program::loadCurrentAddressIntoVariable(int32_t variable_index) {
  appendData1(0x19);
  appendData4(variable_index);
}

void Program::printVariable(int32_t variable_index) {
  appendData1(0x1a);
  appendData4(variable_index);
}

void Program::setStringTableEntry(int32_t string_table_index, std::string string) {
  if (!canFit(3 + string.size())) {
    throw std::runtime_error("Unable to fit instruction into program.");
  }

  appendData1(0x1b);
  appendData2(string.size());
  std::memcpy(&data_[pointer_], string.data(), string.size());
  pointer_ += string.size();
}

void Program::printStringFromStringTable(int32_t string_table_index) {
  appendData1(0x1c);
  appendData4(string_table_index);
}

void Program::loadStringTableLimitIntoVariable(int32_t variable_index) {
  appendData1(0x1d);
  appendData4(variable_index);
}

void Program::terminate(unsigned char return_code) {
  appendData1(0x1e);
  appendData1(return_code);
}

void Program::copyVariable(int32_t source_variable_index, int32_t destination_variable_index) {
  appendData1(0x1f);
  appendData4(source_variable_index);
  appendData4(destination_variable_index);
}

bool Program::canFit(int32_t bytes) {
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

}  // namespace beast
