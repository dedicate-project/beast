#include <beast/random_program_factory.hpp>

// Standard
#include <random>

namespace beast {

Program RandomProgramFactory::generate(
    uint32_t size, uint32_t memory_size, uint32_t /*string_table_size*/,
    uint32_t /*string_table_item_length*/) {
  Program prg(size);

  std::random_device random_device;
  std::mt19937 mersenne_engine{random_device()};
  std::uniform_int_distribution<int32_t> code_distribution(1, 76);
  std::uniform_int_distribution<int32_t> var_distribution(0, memory_size - 1);
  std::uniform_int_distribution<int32_t> bool_distribution(0, 1);

  const auto bool_generator =
      [&bool_distribution, &mersenne_engine]() {
        return bool_distribution(mersenne_engine) == 1;
      };
  const auto var_generator =
      [&var_distribution, &mersenne_engine]() {
        return var_distribution(mersenne_engine);
      };

  while (true) {
    Program fragment;

    const auto code = static_cast<OpCode>(code_distribution(mersenne_engine));
    switch (code) {
    case OpCode::NoOp: {
      fragment.noop();
    } break;

    case OpCode::LoadMemorySizeIntoVariable: {
      fragment.loadMemorySizeIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::LoadCurrentAddressIntoVariable: {
      fragment.loadCurrentAddressIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::Terminate: {
      std::uniform_int_distribution<int32_t> return_code_distribution{-127, 128};
      fragment.terminate(return_code_distribution(mersenne_engine));
    } break;

    default:
      break;
    }

    // TODO(fairlight1337): Implement generation of actual random programs here. Right now, this
    // generator's "random" programs consist solely of no-ops. Also, cover the size of the generated
    // programs as well as the content for specific implementations of the base class by proper
    // tests.

    if (fragment.getSize() + prg.getPointer() > prg.getSize()) {
      break;
    }
    prg.insertProgram(fragment);
  }

  return prg;
}

}  // namespace beast
