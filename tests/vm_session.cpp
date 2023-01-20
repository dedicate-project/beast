#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("when_marked_exited_abnormally_that_status_can_be_retrieved", "vm_session") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setExitedAbnormally();

  REQUIRE(session.didExitAbnormally() == true);
}

TEST_CASE("when_not_marked_exited_abnormally_that_status_can_be_retrieved", "vm_session") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);

  REQUIRE(session.didExitAbnormally() == false);
}
