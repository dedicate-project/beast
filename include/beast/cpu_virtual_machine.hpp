#ifndef BEAST_CPU_VIRTUAL_MACHINE_HPP_
#define BEAST_CPU_VIRTUAL_MACHINE_HPP_

#include <beast/virtual_machine.hpp>

namespace beast {

class CpuVirtualMachine : public VirtualMachine {
 public:
  bool step(VmSession& session) override;
};

}  // namespace beast

#endif  // BEAST_CPU_VIRTUAL_MACHINE_HPP_
