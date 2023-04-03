#include <beast/program.hpp>

// Standard
#include <cstring>
#include <stdexcept>

namespace beast {

Program::Program(uint32_t space) : data_(space, 0x00), grows_dynamically_{false} {}

Program::Program(std::vector<unsigned char> data)
    : data_{std::move(data)}, grows_dynamically_{false} {}

size_t Program::getSize() const noexcept { return data_.size(); }

int32_t Program::getData4(int32_t offset) {
  if ((offset + 4) > getSize()) {
    throw std::underflow_error("Unable to retrieve data (not enough data left).");
  }

  int32_t buffer = 0x0;
  std::memcpy(&buffer, &data_[offset], 4);
  return buffer;
}

int16_t Program::getData2(int32_t offset) {
  if ((offset + 2) > getSize()) {
    throw std::underflow_error("Unable to retrieve data (not enough data left).");
  }

  int16_t buffer = 0x0;
  std::memcpy(&buffer, &data_[offset], 2);
  return buffer;
}

int8_t Program::getData1(int32_t offset) {
  if ((offset + 1) > getSize()) {
    throw std::underflow_error("Unable to retrieve data (not enough data left).");
  }

  int8_t buffer = 0x0;
  std::memcpy(&buffer, &data_[offset], 1);
  return buffer;
}

uint32_t Program::getPointer() const noexcept { return pointer_; }

void Program::insertProgram(const Program& other) {
  const size_t to_fit = other.getSize();
  if (!canFit(static_cast<uint32_t>(to_fit))) {
    throw std::overflow_error("Unable to fit other program into program.");
  }
  for (uint32_t idx = 0; idx < to_fit; ++idx) {
    data_[pointer_ + idx] = other.getData()[idx];
  }
  pointer_ += static_cast<uint32_t>(to_fit);
}

const std::vector<unsigned char>& Program::getData() const noexcept { return data_; }

void Program::noop() { appendCode1(OpCode::NoOp); }

void Program::declareVariable(int32_t variable_index, VariableType variable_type) {
  appendCode1(OpCode::DeclareVariable);
  appendData4(variable_index);
  appendData1(static_cast<int8_t>(variable_type));
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

void Program::addVariableToVariable(int32_t source_variable_index, bool follow_source_links,
                                    int32_t destination_variable_index,
                                    bool follow_destination_links) {
  appendCode1(OpCode::AddVariableToVariable);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::subtractConstantFromVariable(int32_t variable_index, int32_t constant,
                                           bool follow_links) {
  appendCode1(OpCode::SubtractConstantFromVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
}

void Program::subtractVariableFromVariable(int32_t source_variable_index, bool follow_source_links,
                                           int32_t destination_variable_index,
                                           bool follow_destination_links) {
  appendCode1(OpCode::SubtractVariableFromVariable);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address_variable_index,
    bool follow_addr_links) {
  appendCode1(OpCode::RelativeJumpToVariableAddressIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address_variable_index,
    bool follow_addr_links) {
  appendCode1(OpCode::RelativeJumpToVariableAddressIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address_variable_index,
    bool follow_addr_links) {
  appendCode1(OpCode::RelativeJumpToVariableAddressIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address_variable_index,
    bool follow_addr_links) {
  appendCode1(OpCode::AbsoluteJumpToVariableAddressIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address_variable_index,
    bool follow_addr_links) {
  appendCode1(OpCode::AbsoluteJumpToVariableAddressIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address_variable_index,
    bool follow_addr_links) {
  appendCode1(OpCode::AbsoluteJumpToVariableAddressIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address_variable_index);
  appendFlag1(follow_addr_links);
}

void Program::relativeJumpToAddressIfVariableGreaterThanZero(int32_t variable_index,
                                                             bool follow_links,
                                                             int32_t relative_jump_address) {
  appendCode1(OpCode::RelativeJumpIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address);
}

void Program::relativeJumpToAddressIfVariableLessThanZero(int32_t variable_index, bool follow_links,
                                                          int32_t relative_jump_address) {
  appendCode1(OpCode::RelativeJumpIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address);
}

void Program::relativeJumpToAddressIfVariableEqualsZero(int32_t variable_index, bool follow_links,
                                                        int32_t relative_jump_address) {
  appendCode1(OpCode::RelativeJumpIfVariableEq0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(relative_jump_address);
}

void Program::absoluteJumpToAddressIfVariableGreaterThanZero(int32_t variable_index,
                                                             bool follow_links,
                                                             int32_t absolute_jump_address) {
  appendCode1(OpCode::AbsoluteJumpIfVariableGt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address);
}

void Program::absoluteJumpToAddressIfVariableLessThanZero(int32_t variable_index, bool follow_links,
                                                          int32_t absolute_jump_address) {
  appendCode1(OpCode::AbsoluteJumpIfVariableLt0);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(absolute_jump_address);
}

void Program::absoluteJumpToAddressIfVariableEqualsZero(int32_t variable_index, bool follow_links,
                                                        int32_t absolute_jump_address) {
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

void Program::checkIfVariableIsInput(int32_t source_variable_index, bool follow_source_links,
                                     int32_t destination_variable_index,
                                     bool follow_destination_links) {
  appendCode1(OpCode::CheckIfVariableIsInput);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::checkIfVariableIsOutput(int32_t source_variable_index, bool follow_source_links,
                                      int32_t destination_variable_index,
                                      bool follow_destination_links) {
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

void Program::setStringTableEntry(int32_t string_table_index, const std::string& string) {
  if (!canFit(static_cast<uint32_t>(3 + string.size()))) {
    throw std::overflow_error("Unable to fit instruction into program.");
  }

  appendCode1(OpCode::SetStringTableEntry);
  appendData4(string_table_index);
  appendData2(static_cast<int16_t>(string.size()));
  for (char character : string) {
    appendData1(character);
  }
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

void Program::copyVariable(int32_t source_variable_index, bool follow_source_links,
                           int32_t destination_variable_index, bool follow_destination_links) {
  appendCode1(OpCode::CopyVariable);
  appendData4(source_variable_index);
  appendFlag1(follow_source_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::checkIfInputWasSet(int32_t variable_index, bool follow_links,
                                 int32_t destination_variable_index,
                                 bool follow_destination_links) {
  appendCode1(OpCode::CheckIfInputWasSet);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(destination_variable_index);
  appendFlag1(follow_destination_links);
}

void Program::loadStringTableItemLengthLimitIntoVariable(int32_t variable_index,
                                                         bool follow_links) {
  appendCode1(OpCode::LoadStringTableItemLengthLimitIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadRandomValueIntoVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadRandomValueIntoVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::unconditionalJumpToAbsoluteAddress(int32_t addr) {
  appendCode1(OpCode::UnconditionalJumpToAbsoluteAddress);
  appendData4(addr);
}

void Program::unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index,
                                                         bool follow_links) {
  appendCode1(OpCode::UnconditionalJumpToAbsoluteVariableAddress);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::unconditionalJumpToRelativeAddress(int32_t addr) {
  appendCode1(OpCode::UnconditionalJumpToRelativeAddress);
  appendData4(addr);
}

void Program::unconditionalJumpToRelativeVariableAddress(int32_t variable_index,
                                                         bool follow_links) {
  appendCode1(OpCode::UnconditionalJumpToRelativeVariableAddress);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadStringItemLengthIntoVariable(int32_t string_table_index, int32_t variable_index,
                                               bool follow_links) {
  appendCode1(OpCode::LoadStringItemLengthIntoVariable);
  appendData4(string_table_index);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadStringItemIntoVariables(int32_t string_table_index, int32_t start_variable_index,
                                          bool follow_links) {
  appendCode1(OpCode::LoadStringItemIntoVariables);
  appendData4(string_table_index);
  appendData4(start_variable_index);
  appendFlag1(follow_links);
}

void Program::performSystemCall(int8_t major_code, int8_t minor_code, int32_t variable_index,
                                bool follow_links) {
  appendCode1(OpCode::PerformSystemCall);
  appendData1(major_code);
  appendData1(minor_code);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::bitShiftVariableLeft(int32_t variable_index, bool follow_links, int8_t places) {
  appendCode1(OpCode::BitShiftVariableLeft);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData1(places);
}

void Program::bitShiftVariableRight(int32_t variable_index, bool follow_links, int8_t places) {
  appendCode1(OpCode::BitShiftVariableRight);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData1(places);
}

void Program::bitWiseInvertVariable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::BitWiseInvertVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::bitWiseAndTwoVariables(int32_t variable_index_a, bool follow_links_a,
                                     int32_t variable_index_b, bool follow_links_b) {
  appendCode1(OpCode::BitWiseAndTwoVariables);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
}

void Program::bitWiseOrTwoVariables(int32_t variable_index_a, bool follow_links_a,
                                    int32_t variable_index_b, bool follow_links_b) {
  appendCode1(OpCode::BitWiseOrTwoVariables);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
}

void Program::bitWiseXorTwoVariables(int32_t variable_index_a, bool follow_links_a,
                                     int32_t variable_index_b, bool follow_links_b) {
  appendCode1(OpCode::BitWiseXorTwoVariables);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
}

void Program::moduloVariableByConstant(int32_t variable_index, bool follow_links,
                                       int32_t modulo_constant) {
  appendCode1(OpCode::ModuloVariableByConstant);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(modulo_constant);
}

void Program::moduloVariableByVariable(int32_t variable_index, bool follow_links,
                                       int32_t modulo_variable_index, bool modulo_follow_links) {
  appendCode1(OpCode::ModuloVariableByVariable);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(modulo_variable_index);
  appendFlag1(modulo_follow_links);
}

void Program::rotateVariableLeft(int32_t variable_index, bool follow_links, int8_t places) {
  appendCode1(OpCode::RotateVariableLeft);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData1(places);
}

void Program::rotateVariableRight(int32_t variable_index, bool follow_links, int8_t places) {
  appendCode1(OpCode::RotateVariableRight);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData1(places);
}

void Program::pushVariableOnStack(int32_t stack_variable_index, bool follow_links_stack,
                                  int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::PushVariableOnStack);
  appendData4(stack_variable_index);
  appendFlag1(follow_links_stack);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::pushConstantOnStack(int32_t stack_variable_index, bool follow_links_stack,
                                  int32_t constant) {
  appendCode1(OpCode::PushConstantOnStack);
  appendData4(stack_variable_index);
  appendFlag1(follow_links_stack);
  appendData4(constant);
}

void Program::popVariableFromStack(int32_t stack_variable_index, bool follow_links_stack,
                                   int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::PopVariableFromStack);
  appendData4(stack_variable_index);
  appendFlag1(follow_links_stack);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::popTopItemFromStack(int32_t stack_variable_index, bool follow_links_stack) {
  appendCode1(OpCode::PopTopItemFromStack);
  appendData4(stack_variable_index);
  appendFlag1(follow_links_stack);
}

void Program::checkIfStackIsEmpty(int32_t stack_variable_index, bool follow_links_stack,
                                  int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::CheckIfStackIsEmpty);
  appendData4(stack_variable_index);
  appendFlag1(follow_links_stack);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::swapVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b,
                            bool follow_links_b) {
  appendCode1(OpCode::SwapVariables);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
}

void Program::setVariableStringTableEntry(int32_t variable_index, bool follow_links,
                                          const std::string& string) {
  appendCode1(OpCode::SetVariableStringTableEntry);
  appendData4(variable_index);
  appendFlag1(follow_links);

  appendData2(static_cast<int16_t>(string.size()));
  for (char character : string) {
    appendData1(character);
  }
}

void Program::printVariableStringFromStringTable(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::PrintVariableStringFromStringTable);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadVariableStringItemLengthIntoVariable(int32_t string_item_variable_index,
                                                       bool follow_links_string_item,
                                                       int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadVariableStringItemLengthIntoVariable);
  appendData4(string_item_variable_index);
  appendFlag1(follow_links_string_item);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::loadVariableStringItemIntoVariables(int32_t string_item_variable_index,
                                                  bool follow_links_string_item,
                                                  int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::LoadVariableStringItemIntoVariables);
  appendData4(string_item_variable_index);
  appendFlag1(follow_links_string_item);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::terminateWithVariableReturnCode(int32_t variable_index, bool follow_links) {
  appendCode1(OpCode::TerminateWithVariableReturnCode);
  appendData4(variable_index);
  appendFlag1(follow_links);
}

void Program::variableBitShiftVariableLeft(int32_t variable_index, bool follow_links,
                                           int32_t places_variable_index,
                                           bool follow_links_places) {
  appendCode1(OpCode::VariableBitShiftVariableLeft);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(places_variable_index);
  appendFlag1(follow_links_places);
}

void Program::variableBitShiftVariableRight(int32_t variable_index, bool follow_links,
                                            int32_t places_variable_index,
                                            bool follow_links_places) {
  appendCode1(OpCode::VariableBitShiftVariableRight);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(places_variable_index);
  appendFlag1(follow_links_places);
}

void Program::variableRotateVariableLeft(int32_t variable_index, bool follow_links,
                                         int32_t places_variable_index, bool follow_links_places) {
  appendCode1(OpCode::VariableRotateVariableLeft);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(places_variable_index);
  appendFlag1(follow_links_places);
}

void Program::variableRotateVariableRight(int32_t variable_index, bool follow_links,
                                          int32_t places_variable_index, bool follow_links_places) {
  appendCode1(OpCode::VariableRotateVariableRight);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(places_variable_index);
  appendFlag1(follow_links_places);
}

void Program::compareIfVariableGtConstant(int32_t variable_index, bool follow_links,
                                          int32_t constant, int32_t target_variable_index,
                                          bool target_follow_links) {
  appendCode1(OpCode::CompareIfVariableGtConstant);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::compareIfVariableLtConstant(int32_t variable_index, bool follow_links,
                                          int32_t constant, int32_t target_variable_index,
                                          bool target_follow_links) {
  appendCode1(OpCode::CompareIfVariableLtConstant);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::compareIfVariableEqConstant(int32_t variable_index, bool follow_links,
                                          int32_t constant, int32_t target_variable_index,
                                          bool target_follow_links) {
  appendCode1(OpCode::CompareIfVariableEqConstant);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::compareIfVariableGtVariable(int32_t variable_index_a, bool follow_links_a,
                                          int32_t variable_index_b, bool follow_links_b,
                                          int32_t target_variable_index, bool target_follow_links) {
  appendCode1(OpCode::CompareIfVariableGtVariable);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::compareIfVariableLtVariable(int32_t variable_index_a, bool follow_links_a,
                                          int32_t variable_index_b, bool follow_links_b,
                                          int32_t target_variable_index, bool target_follow_links) {
  appendCode1(OpCode::CompareIfVariableLtVariable);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::compareIfVariableEqVariable(int32_t variable_index_a, bool follow_links_a,
                                          int32_t variable_index_b, bool follow_links_b,
                                          int32_t target_variable_index, bool target_follow_links) {
  appendCode1(OpCode::CompareIfVariableEqVariable);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::getMaxOfVariableAndConstant(int32_t variable_index, bool follow_links,
                                          int32_t constant, int32_t target_variable_index,
                                          bool target_follow_links) {
  appendCode1(OpCode::GetMaxOfVariableAndConstant);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::getMinOfVariableAndConstant(int32_t variable_index, bool follow_links,
                                          int32_t constant, int32_t target_variable_index,
                                          bool target_follow_links) {
  appendCode1(OpCode::GetMinOfVariableAndConstant);
  appendData4(variable_index);
  appendFlag1(follow_links);
  appendData4(constant);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::getMaxOfVariableAndVariable(int32_t variable_index_a, bool follow_links_a,
                                          int32_t variable_index_b, bool follow_links_b,
                                          int32_t target_variable_index, bool target_follow_links) {
  appendCode1(OpCode::GetMaxOfVariableAndVariable);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

void Program::getMinOfVariableAndVariable(int32_t variable_index_a, bool follow_links_a,
                                          int32_t variable_index_b, bool follow_links_b,
                                          int32_t target_variable_index, bool target_follow_links) {
  appendCode1(OpCode::GetMinOfVariableAndVariable);
  appendData4(variable_index_a);
  appendFlag1(follow_links_a);
  appendData4(variable_index_b);
  appendFlag1(follow_links_b);
  appendData4(target_variable_index);
  appendFlag1(target_follow_links);
}

bool Program::canFit(uint32_t bytes) {
  if (grows_dynamically_) {
    ensureSize(pointer_ + bytes);
  }
  return bytes <= data_.size() - pointer_;
}

void Program::appendData4(int32_t data) {
  if (!canFit(4)) {
    throw std::overflow_error("Unable to fit data into program.");
  }

  std::memcpy(&data_[pointer_], &data, 4);
  pointer_ += 4;
}

void Program::appendData2(int16_t data) {
  if (!canFit(2)) {
    throw std::overflow_error("Unable to fit data into program.");
  }

  std::memcpy(&data_[pointer_], &data, 2);
  pointer_ += 2;
}

void Program::appendData1(int8_t data) {
  if (!canFit(1)) {
    throw std::overflow_error("Unable to fit data into program.");
  }

  std::memcpy(&data_[pointer_], &data, 1);
  pointer_ += 1;
}

void Program::appendFlag1(bool flag) { appendData1(flag ? 0x1 : 0x0); }

void Program::appendCode1(OpCode opcode) { appendData1(static_cast<int8_t>(opcode)); }

void Program::ensureSize(uint32_t size) noexcept {
  if (data_.size() < size) {
    data_.resize(size);
  }
}

} // namespace beast
