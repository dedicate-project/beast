// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

int main(int /*argc*/, char** /*argv*/) {
  /* Print BEAST library version. */
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version "
            << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "."
            << static_cast<uint32_t>(version[2]) << "." << std::endl;

  // Evolution parameters
  const uint32_t pop_size = 10;
  const uint32_t indiv_size = 100;

  // Program execution environment
  const uint32_t prg_size = 50;
  const uint32_t mem_size = 100;
  const uint32_t string_table_size = 10;
  const uint32_t string_table_item_length = 25;

  beast::Pipe pipe(pop_size, indiv_size);
  beast::RandomProgramFactory factory;

  while (pipe.hasSpace()) {
    beast::Program prg =
        factory.generate(prg_size, mem_size, string_table_size, string_table_item_length);
    beast::VmSession session(
        std::move(prg), mem_size, string_table_size, string_table_item_length);
    pipe.addInput(session);
  }

  pipe.evolve();

  return 0;
}
