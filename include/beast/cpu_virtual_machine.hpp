#ifndef BEAST_CPU_VIRTUAL_MACHINE_HPP_
#define BEAST_CPU_VIRTUAL_MACHINE_HPP_

// Internal
#include <beast/virtual_machine.hpp>

namespace beast {

class CpuVirtualMachine : public VirtualMachine {
 public:
  /**
   * @fn CpuVirtualMachine::~CpuVirtualMachine
   * @brief Virtual destructor performing no operation to ensure vtable consistency
   */
  ~CpuVirtualMachine() override = default;

  [[nodiscard]] bool step(VmSession& session, bool dry_run) override;

 protected:
  void message(MessageSeverity severity, const std::string& message) noexcept override;
};

}  // namespace beast

#endif  // BEAST_CPU_VIRTUAL_MACHINE_HPP_
