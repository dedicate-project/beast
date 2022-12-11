#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <beast/cpu_virtual_machine.hpp>

TEST_CASE("noop", "instructions") {
  beast::Program prg(1);
  prg.noop();

  REQUIRE(prg.getData1(0) == 0x00);
}
