#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("stepping_outside_of_bounds_is_rejected_by_vm", "cpu_vm") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  const bool forelast_step = virtual_machine.step(session, false);
  const bool last_step = virtual_machine.step(session, false);
  const bool rejected = virtual_machine.step(session, false);

  REQUIRE(forelast_step == true);
  REQUIRE(last_step == false);
  REQUIRE(rejected == false);
}

TEST_CASE("stepping_beyond_end_of_program_causes_abnormal_exit", "cpu_vm") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  static_cast<void>(virtual_machine.step(session, false));
  static_cast<void>(virtual_machine.step(session, false));
  static_cast<void>(virtual_machine.step(session, false));

  REQUIRE(session.getRuntimeStatistics().abnormal_exit == true);
}

TEST_CASE("when_invalid_opcode_is_encoutered_vm_throws", "cpu_vm") {
  std::vector<unsigned char> bytecode = {0xff};
  beast::Program prg(std::move(bytecode));

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;

  REQUIRE_THROWS_AS(
      static_cast<void>(virtual_machine.step(session, false)), std::invalid_argument);
}
