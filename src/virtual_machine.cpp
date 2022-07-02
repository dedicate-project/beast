#include <beast/virtual_machine.hpp>

namespace beast {

void VirtualMachine::debug(const std::string& message) {
  this->message(MessageSeverity::Debug, message);
}

void VirtualMachine::info(const std::string& message) {
  this->message(MessageSeverity::Info, message);
}

void VirtualMachine::warning(const std::string& message) {
  this->message(MessageSeverity::Warning, message);
}

void VirtualMachine::error(const std::string& message) {
  this->message(MessageSeverity::Error, message);
}

void VirtualMachine::panic(const std::string& message) {
  this->message(MessageSeverity::Panic, message);
}

}  // namespace beast
