#include <beast/virtual_machine.hpp>

namespace beast {

void VirtualMachine::setMinimumMessageSeverity(MessageSeverity minimum_severity) noexcept {
  minimum_severity_ = minimum_severity;
}

void VirtualMachine::debug(const std::string& message) noexcept {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Debug) && !silent_) {
    this->message(MessageSeverity::Debug, message);
  }
}

void VirtualMachine::info(const std::string& message) noexcept {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Info) && !silent_) {
    this->message(MessageSeverity::Info, message);
  }
}

void VirtualMachine::warning(const std::string& message) noexcept {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Warning) && !silent_) {
    this->message(MessageSeverity::Warning, message);
  }
}

void VirtualMachine::error(const std::string& message) noexcept {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Error) && !silent_) {
    this->message(MessageSeverity::Error, message);
  }
}

void VirtualMachine::panic(const std::string& message) noexcept {
  if (shouldDisplayMessageWithSeverity(MessageSeverity::Panic) && !silent_) {
    this->message(MessageSeverity::Panic, message);
  }
}

bool VirtualMachine::shouldDisplayMessageWithSeverity(MessageSeverity severity) const noexcept {
  return static_cast<uint32_t>(severity) >= static_cast<uint32_t>(minimum_severity_);
}

void VirtualMachine::setSilent(bool silent) { silent_ = silent; }

} // namespace beast
