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
   * @enum class MessageSeverity
   * @brief An enumeration of message severities that denote their urgency.
   *
   * Different message urgencies/severities help distinguish how important a log message is. These
   * severities can be used to communicate to an end user how severely a log message is to be taken,
   * and whether any concerning issues have taken place.
   */
  enum class MessageSeverity {
    Debug = 0,   ///< Messages that are of lowest priority and only meant for debug purposes.
    Info = 1,    ///< Messages that are of informational nature, for denoting normal program flow.
    Warning = 2, ///< Messages that denote abnormal but tolerable program flow.
    Error = 3,   ///< Messages that denote issues that interrupt the normal program flow.
    Panic = 4    ///< Messages that denote an immediate program abort due to fatal conditions.
  };

  /**
   * @fn VirtualMachine::VirtualMachine
   * @brief Constructor for the VirtualMachine base class
   *
   * This is the base class constructor for all virtual machine implementations.
   */
  VirtualMachine() = default;

  /**
   * @fn VirtualMachine::~VirtualMachine
   * @brief Virtual destructor performing no operation to ensure vtable consistency
   */
  virtual ~VirtualMachine() = default;

  /**
   * @fn VirtualMachine::setMinimumMessageSeverity
   * @brief Sets the minimum message severity for displaying messages
   *
   * If a message to be printed is below the minimum severity, it is silently dropped. The default
   * severity is MessageSeverity::Info. To display all messages (including a trace of executed
   * operators and their operands), set the severity to MessageSeverity::Debug.
   */
  void setMinimumMessageSeverity(MessageSeverity minimum_severity) noexcept;

  /**
   * @fn VirtualMachine::step
   * @brief Executes the next step a program in its current state as denoted by a VM session.
   *
   * This function must be implemented by subclasses of the VirtualMachine class. A VmSession is
   * passed in that contains a program to execute, and a state at which the program currently is
   * (including instruction pointer, variable memory, and string table).
   *
   * When the `dry_run` parameter is passed, the VM is supposed to only step through the operations
   * in a program and not actually execute them. This can be used to count the effective amount and
   * individual types of operators in a program, as well as analyze its structure.
   *
   * @param session The VmSession instance that holds the program and state to step through.
   * @param dry_run Determines whether operators are executed or just read.
   * @return A boolean flag denoting whether the session can further execute instructions.
   */
  [[nodiscard]] virtual bool step(VmSession& session, bool dry_run) = 0;

  void setSilent(bool silent);

 protected:
  /**
   * @fn VirtualMachine::message
   * @brief Implements the actual output mechanism for messages with a given severity
   *
   * Implementations of the VirtualMachine base class of BEAST virtual machines are required to
   * implement this function. It receives every message a BEAST program or the VirtualMachine
   * implementation wants to print and should honor the given severity to signal to the user about a
   * message's urgency.
   */
  virtual void message(MessageSeverity severity, const std::string& message) noexcept = 0;

  /**
   * @fn VirtualMachine::debug
   * @brief Prints a message with MessageSeverity::Debug severity
   */
  void debug(const std::string& message) noexcept;

  /**
   * @fn VirtualMachine::info
   * @brief Prints a message with MessageSeverity::Info severity
   */
  void info(const std::string& message) noexcept;

  /**
   * @fn VirtualMachine::warning
   * @brief Prints a message with MessageSeverity::Warning severity
   */
  void warning(const std::string& message) noexcept;

  /**
   * @fn VirtualMachine::error
   * @brief Prints a message with MessageSeverity::Error severity
   */
  void error(const std::string& message) noexcept;

  /**
   * @fn VirtualMachine::panic
   * @brief Prints a message with MessageSeverity::Panic severity
   */
  void panic(const std::string& message) noexcept;

 private:
  /**
   * @fn VirtualMachine::shouldDisplayMessageWithSeverity
   * @brief Determines whether a message with a given severity should be displayed or not
   */
  [[nodiscard]] bool shouldDisplayMessageWithSeverity(MessageSeverity severity) const noexcept;

  /**
   * @var VirtualMachine::minimum_severity_
   * @brief The minimum message severity to print
   *
   * Only messages that are equal or above this severity are actually printed. All other messages
   * are silently dropped.
   */
  MessageSeverity minimum_severity_ = MessageSeverity::Info;

  bool silent_ = false;
};

}  // namespace beast

#endif  // BEAST_VIRTUAL_MACHINE_HPP_
