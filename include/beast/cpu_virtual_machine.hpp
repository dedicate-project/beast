#ifndef BEAST_CPU_VIRTUAL_MACHINE_HPP_
#define BEAST_CPU_VIRTUAL_MACHINE_HPP_

// Internal
#include <beast/virtual_machine.hpp>

namespace beast {

/**
 * @class CpuVirtualMachine
 * @brief Runs program code in a CPU-based environment
 *
 * All operators are executed in a step-by-step CPU-based state machine. No alteration of the
 * program code to execute is applied, and execution is not parallelized. The execution behavior is
 * guaranteed to be deterministic.
 *
 * @author Jan Winkler
 * @date 2022-12-19
 */
class CpuVirtualMachine : public VirtualMachine {
 public:
  [[nodiscard]] bool step(VmSession& session, bool dry_run) override;

 protected:
  void message(MessageSeverity severity, const std::string& message) noexcept override;
};

}  // namespace beast

#endif  // BEAST_CPU_VIRTUAL_MACHINE_HPP_
