#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("stepping_outside_of_bounds_is_rejected_by_vm", "cpu_vm") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  const bool forelast_step = vm.step(session, false);
  const bool last_step = vm.step(session, false);
  const bool rejected = vm.step(session, false);

  REQUIRE(forelast_step == true);
  REQUIRE(last_step == false);
  REQUIRE(rejected == false);
}

TEST_CASE("when_invalid_opcode_is_encoutered_vm_throws", "cpu_vm") {
  std::vector<unsigned char> bytecode = {0xff};
  beast::Program prg(std::move(bytecode));

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  bool threw = false;
  try {
    vm.step(session, false);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}
