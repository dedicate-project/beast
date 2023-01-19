#include <beast/virtual_machine.hpp>

namespace beast {

void VirtualMachine::setMinimumMessageSeverity(MessageSeverity minimum_severity) {
  minimum_severity_ = minimum_severity;
}

void VirtualMachine::debug(const std::string& message) {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Debug)) {
    this->message(MessageSeverity::Debug, message);
  }
}

void VirtualMachine::info(const std::string& message) {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Info)) {
    this->message(MessageSeverity::Info, message);
  }
}

void VirtualMachine::warning(const std::string& message) {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Warning)) {
    this->message(MessageSeverity::Warning, message);
  }
}

void VirtualMachine::error(const std::string& message) {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Error)) {
    this->message(MessageSeverity::Error, message);
  }
}

void VirtualMachine::panic(const std::string& message) {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Panic)) {
    this->message(MessageSeverity::Panic, message);
  }
}

bool VirtualMachine::shouldDisplayMessageWithSeverity(MessageSeverity severity) const {
  return static_cast<uint32_t>(severity) >= static_cast<uint32_t>(minimum_severity_);
}

}  // namespace beast
