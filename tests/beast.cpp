#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_FAST_COMPILE

#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("beast_version_is_not_0_0_0", "beast") {
  const auto version = beast::getVersion();

  // Require at least one portion of the version to be > 0.
  REQUIRE(((version[0] > 0) || (version[1] > 0) || (version[2] > 0)) == true);
}
