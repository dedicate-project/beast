#ifndef BEAST_VIRTUAL_MACHINE_HPP_
#define BEAST_VIRTUAL_MACHINE_HPP_

#include <beast/vm_session.hpp>

namespace beast {

class VirtualMachine {
 public:
  virtual bool step(VmSession& session) = 0;

 protected:
  enum class MessageSeverity {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Panic = 4
  };

  virtual void message(MessageSeverity severity, const std::string& message) = 0;

  void debug(const std::string& message);
  void info(const std::string& message);
  void warning(const std::string& message);
  void error(const std::string& message);
  void panic(const std::string& message);
};

}  // namespace beast

#endif  // BEAST_VIRTUAL_MACHINE_HPP_
