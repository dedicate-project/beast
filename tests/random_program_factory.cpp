#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("random_programs_have_the_right_size", "random_program_factory") {
  beast::RandomProgramFactory factory;

  const uint32_t random_program_size_1 = 60;
  const uint32_t random_program_size_2 = 22;
  const uint32_t random_program_size_3 = 513;

  beast::Program program_1 = factory.generate(random_program_size_1);
  beast::Program program_2 = factory.generate(random_program_size_2);
  beast::Program program_3 = factory.generate(random_program_size_3);

  REQUIRE(program_1.getSize() == random_program_size_1);
  REQUIRE(program_2.getSize() == random_program_size_2);
  REQUIRE(program_3.getSize() == random_program_size_3);
}
