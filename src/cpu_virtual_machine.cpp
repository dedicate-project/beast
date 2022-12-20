#include <beast/cpu_virtual_machine.hpp>

#include <chrono>
#include <ctime>
#include <iostream>

#include <beast/opcodes.hpp>

namespace beast {

bool CpuVirtualMachine::step(VmSession& session) {
  OpCode instruction = OpCode::NoOp;
  try {
    // Try to get next major instruction symbol.
    instruction = static_cast<OpCode>(session.getData1());
  } catch(...) {
    // Unable to get more data; the program came to an unexpected end.
    panic("Program ended unexpectedly.");
    return false;
  }

  switch (instruction) {
  case OpCode::NoOp:
    break;

  case OpCode::DeclareVariable: {
    const int32_t variable_index = session.getData4();
    const auto variable_type = static_cast<Program::VariableType>(session.getData1());
    debug("register_variable(" + std::to_string(variable_index) + ", " + std::to_string(static_cast<int32_t>(variable_type)) + ")");
    session.registerVariable(variable_index, variable_type);
  } break;

  case OpCode::SetVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t variable_content = session.getData4();
    debug("set_variable(" + std::to_string(variable_index) + ", " + std::to_string(variable_content) + ", " + (follow_links ? "true" : "false") + ")");
    session.setVariable(variable_index, variable_content, follow_links);
  } break;

  case OpCode::UndeclareVariable: {
    const int32_t variable_index = session.getData4();
    debug("undeclare_variable(" + std::to_string(variable_index) + ")");
    session.unregisterVariable(variable_index);
  } break;

  case OpCode::AddConstantToVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    debug("add_constant_to_variable(" + std::to_string(variable_index) + ", " + (follow_links ? "true" : "false") + ", " + std::to_string(constant) + ")");
    session.addConstantToVariable(variable_index, constant, follow_links);
  } break;

  case OpCode::AddVariableToVariable: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("add_variable_to_variable(" + std::to_string(source_variable) + ", " + (follow_source_links ? "true" : "false") + ", " + std::to_string(destination_variable) + ", " + (follow_destination_links ? "true" : "false") + ")");
    session.addVariableToVariable(source_variable, destination_variable, follow_source_links, follow_destination_links);
  } break;

  case OpCode::SubtractConstantFromVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    debug("subtract_constant_from_variable(" + std::to_string(variable_index) + ", " + (follow_links ? "true" : "false") + ", " + std::to_string(constant) + ")");
    session.subtractConstantFromVariable(variable_index, constant, follow_links);
  } break;

  case OpCode::SubtractVariableFromVariable: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("subtract_variable_from_variable(" + std::to_string(source_variable) + ", " + (follow_source_links ? "true" : "false") + ", " + std::to_string(destination_variable) + ", " + (follow_destination_links ? "true" : "false") + ")");
    session.subtractVariableFromVariable(source_variable, destination_variable, follow_source_links, follow_destination_links);
  } break;

  case OpCode::RelativeJumpToVariableAddressIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("relative_jump_to_variable_address_if_variable_gt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr_variable) + ", " + (follow_addr_links ? "true" : "false") + ")");
    session.relativeJumpToVariableAddressIfVariableGt0(condition_variable, follow_condition_links, addr_variable, follow_addr_links);
  } break;

  case OpCode::RelativeJumpToVariableAddressIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("relative_jump_to_variable_address_if_variable_lt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr_variable) + ", " + (follow_addr_links ? "true" : "false") + ")");
    session.relativeJumpToVariableAddressIfVariableLt0(condition_variable, follow_condition_links, addr_variable, follow_addr_links);
  } break;

  case OpCode::RelativeJumpToVariableAddressIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("relative_jump_to_variable_address_if_variable_eq_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr_variable) + ", " + (follow_addr_links ? "true" : "false") + ")");
    session.relativeJumpToVariableAddressIfVariableEq0(condition_variable, follow_condition_links, addr_variable, follow_addr_links);
  } break;

  case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("absolute_jump_to_variable_address_if_variable_gt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr_variable) + ", " + (follow_addr_links ? "true" : "false") + ")");
    session.absoluteJumpToVariableAddressIfVariableGt0(condition_variable, follow_condition_links, addr_variable, follow_addr_links);
  } break;

  case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("absolute_jump_to_variable_address_if_variable_lt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr_variable) + ", " + (follow_addr_links ? "true" : "false") + ")");
    session.absoluteJumpToVariableAddressIfVariableLt0(condition_variable, follow_condition_links, addr_variable, follow_addr_links);
  } break;

  case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("absolute_jump_to_variable_address_if_variable_eq_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr_variable) + ", " + (follow_addr_links ? "true" : "false") + ")");
    session.absoluteJumpToVariableAddressIfVariableEq0(condition_variable, follow_condition_links, addr_variable, follow_addr_links);
  } break;

  case OpCode::RelativeJumpIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("relative_jump_to_address_if_variable_gt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr) + ")");
    session.relativeJumpToAddressIfVariableGt0(condition_variable, follow_condition_links, addr);
  } break;

  case OpCode::RelativeJumpIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("relative_jump_to_address_if_variable_lt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr) + ")");
    session.relativeJumpToAddressIfVariableLt0(condition_variable, follow_condition_links, addr);
  } break;

  case OpCode::RelativeJumpIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("relative_jump_to_address_if_variable_eq_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr) + ")");
    session.relativeJumpToAddressIfVariableEq0(condition_variable, follow_condition_links, addr);
  } break;

  case OpCode::AbsoluteJumpIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("absolute_jump_to_address_if_variable_gt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr) + ")");
    session.absoluteJumpToAddressIfVariableGt0(condition_variable, follow_condition_links, addr);
  } break;

  case OpCode::AbsoluteJumpIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("absolute_jump_to_address_if_variable_lt_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr) + ")");
    session.absoluteJumpToAddressIfVariableLt0(condition_variable, follow_condition_links, addr);
  } break;

  case OpCode::AbsoluteJumpIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("absolute_jump_to_address_if_variable_eq_0(" + std::to_string(condition_variable) + ", " + (follow_condition_links ? "true" : "false") + ", " + std::to_string(addr) + ")");
    session.absoluteJumpToAddressIfVariableEq0(condition_variable, follow_condition_links, addr);
  } break;

  case OpCode::LoadMemorySizeIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_memory_size_into_variable(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadMemorySizeIntoVariable(variable, follow_links);
  } break;

  case OpCode::CheckIfVariableIsInput: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("check_if_variable_is_input(" + std::to_string(source_variable) + ", " + (follow_source_links ? "true" : "false") + ", " + std::to_string(destination_variable) + ", " + (follow_destination_links ? "true" : "false") + ")");
    session.checkIfVariableIsInput(source_variable, follow_source_links, destination_variable, follow_destination_links);
  } break;

  case OpCode::CheckIfVariableIsOutput: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("check_if_variable_is_output(" + std::to_string(source_variable) + ", " + (follow_source_links ? "true" : "false") + ", " + std::to_string(destination_variable) + ", " + (follow_destination_links ? "true" : "false") + ")");
    session.checkIfVariableIsOutput(source_variable, follow_source_links, destination_variable, follow_destination_links);
  } break;

  case OpCode::LoadInputCountIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_input_count_into_variable(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadInputCountIntoVariable(variable, follow_links);
  } break;

  case OpCode::LoadOutputCountIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_output_count_into_variable(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadOutputCountIntoVariable(variable, follow_links);
  } break;

  case OpCode::LoadCurrentAddressIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_current_address_into_variable(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadCurrentAddressIntoVariable(variable, follow_links);
  } break;

  case OpCode::PrintVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const bool as_char = session.getData1() != 0x0;
    debug("print_variable(" + std::to_string(variable_index) + ", " + (follow_links ? "true" : "false") + ", " + (as_char ? "true" : "false") + ")");
    session.appendVariableToPrintBuffer(variable_index, follow_links, as_char);
  } break;

  case OpCode::SetStringTableEntry: {
    const int32_t string_table_index = session.getData4();
    const int16_t string_length = session.getData2();
    std::vector<char> buffer(string_length);

    #pragma unroll
    for (unsigned int idx = 0; idx < string_length; ++idx) {
      buffer[idx] = session.getData1();
    }
    const std::string string_content = std::string(buffer.data(), string_length);
    debug("set_string_table_entry(" + std::to_string(string_table_index) + ", " + std::to_string(string_length) + ", '" + string_content + "')");
    session.setStringTableEntry(string_table_index, string_content);
  } break;

  case OpCode::PrintStringFromStringTable: {
    const int32_t string_table_index = session.getData4();
    debug("print_string_from_string_table(" + std::to_string(string_table_index) + ")");
    session.appendToPrintBuffer(session.getStringTableEntry(string_table_index));
  } break;

  case OpCode::LoadStringTableLimitIntoVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_string_table_limit_into_variable(" + std::to_string(variable_index) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadStringTableLimitIntoVariable(variable_index, follow_links);
  } break;

  case OpCode::Terminate: {
    const int8_t return_code = session.getData1();
    debug("terminate(" + std::to_string(return_code) + ")");
    session.terminate(return_code);
  } break;

  case OpCode::CopyVariable: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("copy_variable(" + std::to_string(source_variable) + ", " + (follow_source_links ? "true" : "false") + ", " + std::to_string(destination_variable) + ", " + (follow_destination_links ? "true" : "false") + ")");
    session.copyVariable(source_variable, follow_source_links, destination_variable, follow_destination_links);
  } break;

  case OpCode::LoadStringItemLengthIntoVariable: {
    const int32_t string_table_index = session.getData4();
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_string_item_length_into_variable(" + std::to_string(string_table_index) + ", " + std::to_string(variable_index) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadStringItemLengthIntoVariable(string_table_index, variable_index, follow_links);
  } break;

  /*case OpCode::LoadStringItemIntoVariables: {
    // Todo: Implement
  } break;*/

  /*case OpCode::PerformSystemCall: {
    // Todo: Implement
  } break;*/

  /*case OpCode::BitShiftVariableLeft: {
    // Todo: Implement
  } break;*/

  /*case OpCode::BitShiftVariableRight: {
    // Todo: Implement
  } break;*/

  /*case OpCode::BitWiseInvertVariable: {
    // Todo: Implement
  } break;*/

  /*case OpCode::BitWiseAndTwoVariables: {
    // Todo: Implement
  } break;*/

  /*case OpCode::BitWiseOrTwoVariables: {
    // Todo: Implement
  } break;*/

  /*case OpCode::BitWiseXorTwoVariables: {
    // Todo: Implement
  } break;*/

  case OpCode::LoadRandomValueIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_random_value_into_variable(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadRandomValueIntoVariable(variable, follow_links);
  } break;

  /*case OpCode::ModuloVariableByConstant: {
    // Todo: Implement
  } break;*/

  /*case OpCode::ModuloVariableByVariable: {
    // Todo: Implement
  } break;*/

  /*case OpCode::RotateVariableLeft: {
    // Todo: Implement
  } break;*/

  /*case OpCode::RotateVariableRight: {
    // Todo: Implement
  } break;*/

  case OpCode::UnconditionalJumpToAbsoluteAddress: {
    const int32_t addr = session.getData4();
    debug("unconditional_jump_to_absolute_address(" + std::to_string(addr) + ")");
    session.unconditionalJumpToAbsoluteAddress(addr);
  } break;

  case OpCode::UnconditionalJumpToAbsoluteVariableAddress: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("unconditional_jump_to_absolute_variable_address(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.unconditionalJumpToAbsoluteVariableAddress(variable, follow_links);
  } break;

  case OpCode::UnconditionalJumpToRelativeAddress: {
    const int32_t addr = session.getData4();
    debug("unconditional_jump_to_relative_address(" + std::to_string(addr) + ")");
    session.unconditionalJumpToRelativeAddress(addr);
  } break;

  case OpCode::UnconditionalJumpToRelativeVariableAddress: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("unconditional_jump_to_relative_variable_address(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.unconditionalJumpToRelativeVariableAddress(variable, follow_links);
  } break;

  case OpCode::CheckIfInputWasSet: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("check_if_input_was_set(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ", " + std::to_string(destination_variable) + ", " + (follow_destination_links ? "true" : "false") + ")");
    session.checkIfInputWasSet(variable, follow_links, destination_variable, follow_destination_links);
  } break;

  case OpCode::LoadStringTableItemLengthLimitIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_string_table_item_length_limit_into_variable(" + std::to_string(variable) + ", " + (follow_links ? "true" : "false") + ")");
    session.loadStringTableItemLengthLimitIntoVariable(variable, follow_links);
  } break;

  /*case OpCode::PushVariableOnStack: {
    // Todo: Implement
  } break;*/

  /*case OpCode::PushConstantOnStack: {
    // Todo: Implement
  } break;*/

  /*case OpCode::PopVariableFromStack: {
    // Todo: Implement
  } break;*/

  /*case OpCode::PopFromStack: {
    // Todo: Implement
  } break;*/

  /*case OpCode::CheckIfStackIsEmpty: {
    // Todo: Implement
  } break;*/

  /*case OpCode::SwapVariables: {
    // Todo: Implement
  } break;*/

  /*case OpCode::SetVariableStringTableEntry: {
    // Todo: Implement
  } break;*/

  /*case OpCode::PrintVariableStringFromStringTable: {
    // Todo: Implement
  } break;*/

  /*case OpCode::LoadVariableStringItemLengthIntoVariable: {
    // Todo: Implement
  } break;*/

  /*case OpCode::LoadVariableStringItemIntoVariables: {
    // Todo: Implement
  } break;*/

  /*case OpCode::TerminateWithVariableReturnCode: {
    // Todo: Implement
  } break;*/

  /*case OpCode::VariableBitShiftVariableLeft: {
    // Todo: Implement
  } break;*/

  /*case OpCode::VariableBitShiftVariableRight: {
    // Todo: Implement
  } break;*/

  /*case OpCode::VariableRotateVariableLeft: {
    // Todo: Implement
  } break;*/

  /*case OpCode::VariableRotateVariableRight: {
    // Todo: Implement
  } break;*/

  default: {
    throw std::runtime_error("Undefined instruction reached.");
  }
  }

  return !session.isAtEnd();
}

void CpuVirtualMachine::message(MessageSeverity severity, const std::string& message) {
  std::ostream& out_stream = std::cout;
  uint32_t color_fg = 0;
  uint32_t color_bg = 0;
  std::string prefix;

  switch (severity) {
  case MessageSeverity::Debug: {
    color_fg = 90;
    prefix = "DBG";
  } break;

  case MessageSeverity::Info: {
    color_fg = 97;
    prefix = "INF";
  } break;

  case MessageSeverity::Warning: {
    color_fg = 33;
    prefix = "WRN";
  } break;

  case MessageSeverity::Error: {
    color_fg = 31;
    prefix = "ERR";
  } break;

  case MessageSeverity::Panic: {
    color_fg = 31;
    color_bg = 107;
    prefix = "PNC";
  } break;
  }

  const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::vector<char> timestamp_buffer(100);

  struct tm result{};
  const size_t timestamp_length = std::strftime(timestamp_buffer.data(), 100, "%F %T", localtime_r(&time, &result));
  const std::string timestamp(timestamp_buffer.data(), timestamp_length);
  out_stream << "\033[1;" << color_fg << (color_bg != 0 ? ";" + std::to_string(color_bg) : "") << "m"
             << "[" << timestamp << " " << prefix << "] "
             << message
             << "\033[0m" << std::endl;
}

}  // namespace beast
