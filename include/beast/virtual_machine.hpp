#ifndef BEAST_VIRTUAL_MACHINE_HPP_
#define BEAST_VIRTUAL_MACHINE_HPP_

#include <beast/vm_session.hpp>

namespace beast {

class VirtualMachine {
 public:
  virtual bool step(VmSession& session) = 0;
};

}  // namespace beast

#endif  // BEAST_VIRTUAL_MACHINE_HPP_
