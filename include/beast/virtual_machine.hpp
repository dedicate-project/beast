#ifndef BEAST_VIRTUAL_MACHINE_HPP_
#define BEAST_VIRTUAL_MACHINE_HPP_

// Internal
#include <beast/vm_session.hpp>

namespace beast {

/**
 * @class VirtualMachine
 * @brief A base class for virtual machine types to execute BEAST code.
 *
 * This base class provides messaging functionalities and a base function signature to allow
 * integrating different types of virtual machines that are executing BEAST instructions.
 *
 * @author Jan Winkler
 * @date 2022-12-19
 */
class VirtualMachine {
 public:
  /**
   * @fn VirtualMachine::step
   * @brief Executes the next step a program in its current state as denoted by a VM session.
   *
   * This function must be implemented by subclasses of the VirtualMachine class. A VmSession is
   * passed in that contains a program to execute, and a state at which the program currently is
   * (including instruction pointer, variable memory, and string table).
   *
   * @param session The VmSession instance that holds the program and state to step through.
   * @return A boolean flag denoting whether the session can further execute instructions.
   */
  virtual bool step(VmSession& session) = 0;

 protected:
  /**
   * @enum class MessageSeverity
   * @brief An enumeration of message severities that denote their urgency.
   *
   * Different message urgencies/severities help distinguish how important a log message is. These
   * severities can be used to communicate to an end user how severely a log message is to be taken,
   * and whether any concerning issues have taken place.
   *
   */
  enum class MessageSeverity {
    Debug = 0,   ///< Messages that are of lowest priority and only meant for debug purposes.
    Info = 1,    ///< Messages that are of informational nature, for denoting normal program flow.
    Warning = 2, ///< Messages that denote abnormal but tolerable program flow.
    Error = 3,   ///< Messages that denote issues that interrupt the normal program flow.
    Panic = 4    ///< Messages that denote an immediate program abort due to fatal conditions.
  };

  /**
   * @fn VirtualMachine::message
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  virtual void message(MessageSeverity severity, const std::string& message) = 0;

  /**
   * @fn VirtualMachine::debug
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void debug(const std::string& message);

  /**
   * @fn VirtualMachine::info
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void info(const std::string& message);

  /**
   * @fn VirtualMachine::warning
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void warning(const std::string& message);

  /**
   * @fn VirtualMachine::error
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void error(const std::string& message);

  /**
   * @fn VirtualMachine::panic
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void panic(const std::string& message);
};

}  // namespace beast

#endif  // BEAST_VIRTUAL_MACHINE_HPP_
