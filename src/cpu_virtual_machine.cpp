#include <beast/cpu_virtual_machine.hpp>

#include <chrono>
#include <ctime>
#include <iostream>

namespace beast {

bool CpuVirtualMachine::step(VmSession& session) {
  uint8_t instruction = 0x0;
  try {
    // Try to get next major instruction symbol.
    instruction = session.getData1();
  } catch(...) {
    // Unable to get more data; the program came to an unexpected end.
    panic("Program ended unexpectedly.");
    return false;
  }

  switch (instruction) {
  case 0x00:  // noop
    break;

  case 0x01: {  // declare variable
    const int32_t variable_index = session.getData4();
    const auto variable_type = static_cast<Program::VariableType>(session.getData1());
    debug("register_variable(" + std::to_string(variable_index) + ", " + std::to_string(static_cast<int32_t>(variable_type)) + ")");
    session.registerVariable(variable_index, variable_type);
  } break;

  case 0x02: {  // set variable
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t variable_content = session.getData4();
    debug("set_variable(" + std::to_string(variable_index) + ", " + std::to_string(variable_content) + ", " + (follow_links ? "true" : "false") + ")");
    session.setVariable(variable_index, variable_content, follow_links);
  } break;

  case 0x03: {  // undeclare variable
    const int32_t variable_index = session.getData4();
    debug("undeclare_variable(" + std::to_string(variable_index) + ")");
    session.unregisterVariable(variable_index);
  } break;

  /*case 0x04: {  // add constant to variable
    // Todo: Implement
  } break;

  case 0x05: {  // add variable to variable
    // Todo: Implement
  } break;

  case 0x06: {  // subtract constant from variable
    // Todo: Implement
  } break;

  case 0x07: {  // subtract variable from variable
    // Todo: Implement
  } break;

  case 0x08: {  // rel jump to var addr if variable > 0
    // Todo: Implement
  } break;

  case 0x09: {  // rel jump to var addr if variable < 0
    // Todo: Implement
  } break;

  case 0x0a: {  // rel jump to var addr if variable = 0
    // Todo: Implement
  } break;

  case 0x0b: {  // abs jump to var addr if variable > 0
    // Todo: Implement
  } break;

  case 0x0c: {  // abs jump to var addr if variable < 0
    // Todo: Implement
  } break;

  case 0x0d: {  // abs jump to var addr if variable = 0
    // Todo: Implement
  } break;

  case 0x0e: {  // rel jump if variable > 0
    // Todo: Implement
  } break;

  case 0x0f: {  // rel jump if variable < 0
    // Todo: Implement
  } break;

  case 0x10: {  // rel jump if variable = 0
    // Todo: Implement
  } break;

  case 0x11: {  // abs jump if variable > 0
    // Todo: Implement
  } break;

  case 0x12: {  // abs jump if variable < 0
    // Todo: Implement
  } break;

  case 0x13: {  // abs jump if variable = 0
    // Todo: Implement
  } break;

  case 0x14: {  // load memory size into variable
    // Todo: Implement
  } break;

  case 0x15: {  // check if variable is input
    // Todo: Implement
  } break;

  case 0x16: {  // check if variable is output
    // Todo: Implement
  } break;

  case 0x17: {  // load input count into variable
    // Todo: Implement
  } break;

  case 0x18: {  // load output count into variable
    // Todo: Implement
  } break;

  case 0x19: {  // load current address into variable
    // Todo: Implement
  } break;*/

  case 0x1a: {  // print variable
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("print_variable(" + std::to_string(variable_index) + ", " + (follow_links ? "true" : "false") + ")");
    session.appendVariableToPrintBuffer(variable_index, follow_links);
    // Todo: Implement
  } break;

  case 0x1b: {  // set string table entry
    const int32_t string_table_index = session.getData4();
    const int16_t string_length = session.getData2();
    std::vector<char> buffer(string_length);
    for (unsigned int idx = 0; idx < string_length; ++idx) {
      buffer[idx] = session.getData1();
    }
    const std::string string_content = std::string(buffer.data(), string_length);
    debug("set_string_table_entry(" + std::to_string(string_table_index) + ", " + std::to_string(string_length) + ", '" + string_content + "')");
    session.setStringTableEntry(string_table_index, string_content);
  } break;

  case 0x1c: {  // print string from string table
    const int32_t string_table_index = session.getData4();
    debug("print_string_from_string_table(" + std::to_string(string_table_index) + ")");
    session.appendToPrintBuffer(session.getStringTableEntry(string_table_index));
  } break;

  /*case 0x1d: {  // load string table limit into variable
    // Todo: Implement
  } break;

  case 0x1e: {  // terminate
    // Todo: Implement
  } break;

  case 0x1f: {  // copy variable
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
  const size_t timestamp_length = std::strftime(timestamp_buffer.data(), 100, "%F %T", localtime(&time));
  const std::string timestamp(timestamp_buffer.data(), timestamp_length);
  out_stream << "\033[1;" << color_fg << (color_bg != 0 ? ";" + std::to_string(color_bg) : "") << "m"
             << "[" << timestamp << " " << prefix << "] "
             << message
             << "\033[0m" << std::endl;
}

}  // namespace beast
