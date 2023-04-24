#ifndef BEAST_CPU_VIRTUAL_MACHINE_HPP_
#define BEAST_CPU_VIRTUAL_MACHINE_HPP_

// Internal
#include <beast/virtual_machine.hpp>

namespace beast {

/**
 * @class CpuVirtualMachine
 * @brief Runs program code in a CPU-based environment
 *
 * This class represents a virtual machine that executes program code in a step-by-step,
 * CPU-based state machine. No alteration of the program code to execute is applied, and execution
 * is not parallelized. The execution behavior is guaranteed to be deterministic.
 *
 * @note This class does not provide any public methods beyond the ones inherited from its base
 * class, as it is intended to act as an implementation detail of the BEAST library.
 *
 * @author Jan Winkler
 * @date 2022-12-19
 */
class CpuVirtualMachine : public VirtualMachine {
 public:
  /**
   * @brief Executes a single step of the virtual machine
   *
   * @param session The VmSession object containing the program code to execute
   * @param dry_run Whether to perform a dry run of the program code execution
   *
   * @return true if the execution of the program code should continue, false if the program code
   * has terminated
   */
  [[nodiscard]] bool step(VmSession& session, bool dry_run) override;

 protected:
  /**
   * @brief Outputs a message to the virtual machine's message queue
   *
   * @param severity The severity of the message
   * @param message The message text
   */
  void message(MessageSeverity severity, const std::string& message) noexcept override;
};

} // namespace beast

#endif // BEAST_CPU_VIRTUAL_MACHINE_HPP_
