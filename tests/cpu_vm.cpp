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
  (void)virtual_machine.step(session, false);
  (void)virtual_machine.step(session, false);
  (void)virtual_machine.step(session, false);

  REQUIRE(session.getRuntimeStatistics().abnormal_exit == true);
}

TEST_CASE("when_invalid_opcode_is_encoutered_vm_throws", "cpu_vm") {
  std::vector<unsigned char> bytecode = {0xff};
  beast::Program prg(std::move(bytecode));

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  bool threw = false;
  try {
    (void)virtual_machine.step(session, false);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}
