#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("random_programs_have_the_right_size", "random_program_factory") {
  beast::RandomProgramFactory factory;

  const uint32_t random_program_size_1 = 60;
  const uint32_t random_program_size_2 = 22;
  const uint32_t random_program_size_3 = 513;

  beast::Program program_1 = factory.generate(random_program_size_1, 10, 10, 10);
  beast::Program program_2 = factory.generate(random_program_size_2, 10, 10, 10);
  beast::Program program_3 = factory.generate(random_program_size_3, 10, 10, 10);

  REQUIRE(program_1.getSize() == random_program_size_1);
  REQUIRE(program_2.getSize() == random_program_size_2);
  REQUIRE(program_3.getSize() == random_program_size_3);
}

TEST_CASE("factory_generates_many_large_programs_without_hanging", "random_program_factory") {
  beast::RandomProgramFactory factory;

  const uint32_t random_program_size = 2500;
  const uint32_t program_count = 1000;

  for (uint32_t idx = 0; idx < program_count; ++idx) {
    beast::Program program = factory.generate(random_program_size, 100, 100, 100);
    REQUIRE(program.getSize() == random_program_size);
  }
}
