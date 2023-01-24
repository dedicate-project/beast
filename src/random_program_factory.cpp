#include <beast/random_program_factory.hpp>

namespace beast {

Program RandomProgramFactory::generate(size_t size) {
  Program prg(size);

  // TODO(fairlight1337): Implement generation of actual random programs here. Right now, this
  // generator's "random" programs consist solely of no-ops. Also, cover the size of the generated
  // programs as well as the content for specific implementations of the base class by proper tests.

  return std::move(prg);
}

}  // namespace beast
