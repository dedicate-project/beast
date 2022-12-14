#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("stepping_outside_of_bounds_is_rejected_by_vm", "programs") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine vm;
  const bool forelast_step = vm.step(session);
  const bool last_step = vm.step(session);
  const bool rejected = vm.step(session);

  REQUIRE(forelast_step == true);
  REQUIRE(last_step == false);
  REQUIRE(rejected == false);
}
