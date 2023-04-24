#include <catch2/catch.hpp>

#include <beast/beast.hpp>

namespace {

class MockVirtualMachine : public beast::VirtualMachine {
 public:
  void sendDebugMessage(const std::string& message) noexcept { debug(message); }

  void sendInfoMessage(const std::string& message) noexcept { info(message); }

  void sendWarningMessage(const std::string& message) noexcept { warning(message); }

  void sendErrorMessage(const std::string& message) noexcept { error(message); }

  void sendPanicMessage(const std::string& message) noexcept { panic(message); }

  const std::string& getCachedMessage(beast::VirtualMachine::MessageSeverity severity) noexcept {
    return cache_[severity];
  }

  bool step(beast::VmSession& /*session*/, bool /*dry_run*/) override { return true; }

 protected:
  void message(beast::VirtualMachine::MessageSeverity severity,
               const std::string& message) noexcept override {
    cache_[severity] = message;
  }

 private:
  std::map<beast::VirtualMachine::MessageSeverity, std::string> cache_;
};

} // namespace

TEST_CASE("when_message_severity_set_to_debug_all_messages_are_printed", "virtual_machine") {
  const std::string debug_message = "Debug";
  const std::string info_message = "Info";
  const std::string warning_message = "Warning";
  const std::string error_message = "Error";
  const std::string panic_message = "Panic";

  MockVirtualMachine mock_vm;
  mock_vm.setMinimumMessageSeverity(beast::VirtualMachine::MessageSeverity::Debug);
  mock_vm.sendDebugMessage(debug_message);
  mock_vm.sendInfoMessage(info_message);
  mock_vm.sendWarningMessage(warning_message);
  mock_vm.sendErrorMessage(error_message);
  mock_vm.sendPanicMessage(panic_message);

  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Debug) == debug_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Info) == info_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Warning) ==
          warning_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Error) == error_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Panic) == panic_message);
}

TEST_CASE("when_message_severity_set_to_info_the_right_messages_are_printed", "virtual_machine") {
  const std::string debug_message = "Debug";
  const std::string info_message = "Info";
  const std::string warning_message = "Warning";
  const std::string error_message = "Error";
  const std::string panic_message = "Panic";

  MockVirtualMachine mock_vm;
  mock_vm.setMinimumMessageSeverity(beast::VirtualMachine::MessageSeverity::Info);
  mock_vm.sendDebugMessage(debug_message);
  mock_vm.sendInfoMessage(info_message);
  mock_vm.sendWarningMessage(warning_message);
  mock_vm.sendErrorMessage(error_message);
  mock_vm.sendPanicMessage(panic_message);

  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Debug).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Info) == info_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Warning) ==
          warning_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Error) == error_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Panic) == panic_message);
}

TEST_CASE("when_message_severity_set_to_warn_the_right_messages_are_printed", "virtual_machine") {
  const std::string debug_message = "Debug";
  const std::string info_message = "Info";
  const std::string warning_message = "Warning";
  const std::string error_message = "Error";
  const std::string panic_message = "Panic";

  MockVirtualMachine mock_vm;
  mock_vm.setMinimumMessageSeverity(beast::VirtualMachine::MessageSeverity::Warning);
  mock_vm.sendDebugMessage(debug_message);
  mock_vm.sendInfoMessage(info_message);
  mock_vm.sendWarningMessage(warning_message);
  mock_vm.sendErrorMessage(error_message);
  mock_vm.sendPanicMessage(panic_message);

  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Debug).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Info).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Warning) ==
          warning_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Error) == error_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Panic) == panic_message);
}

TEST_CASE("when_message_severity_set_to_error_the_right_messages_are_printed", "virtual_machine") {
  const std::string debug_message = "Debug";
  const std::string info_message = "Info";
  const std::string warning_message = "Warning";
  const std::string error_message = "Error";
  const std::string panic_message = "Panic";

  MockVirtualMachine mock_vm;
  mock_vm.setMinimumMessageSeverity(beast::VirtualMachine::MessageSeverity::Error);
  mock_vm.sendDebugMessage(debug_message);
  mock_vm.sendInfoMessage(info_message);
  mock_vm.sendWarningMessage(warning_message);
  mock_vm.sendErrorMessage(error_message);
  mock_vm.sendPanicMessage(panic_message);

  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Debug).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Info).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Warning).empty() ==
          true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Error) == error_message);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Panic) == panic_message);
}

TEST_CASE("when_message_severity_set_to_panic_the_right_messages_are_printed", "virtual_machine") {
  const std::string debug_message = "Debug";
  const std::string info_message = "Info";
  const std::string warning_message = "Warning";
  const std::string error_message = "Error";
  const std::string panic_message = "Panic";

  MockVirtualMachine mock_vm;
  mock_vm.setMinimumMessageSeverity(beast::VirtualMachine::MessageSeverity::Panic);
  mock_vm.sendDebugMessage(debug_message);
  mock_vm.sendInfoMessage(info_message);
  mock_vm.sendWarningMessage(warning_message);
  mock_vm.sendErrorMessage(error_message);
  mock_vm.sendPanicMessage(panic_message);

  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Debug).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Info).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Warning).empty() ==
          true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Error).empty() == true);
  REQUIRE(mock_vm.getCachedMessage(beast::VirtualMachine::MessageSeverity::Panic) == panic_message);
}
