#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_FAST_COMPILE

#include <catch2/catch.hpp>

/* This file is used to optimize linking speed for Catch2 tests. It implements
   the main function provided by CATCH_CONFIG_MAIN and is then linked into the
   individual tests.

   For Reference, see:
   https://github.com/catchorg/Catch2/blob/v2.x/docs/slow-compiles.md */
