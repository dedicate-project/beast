// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

class SimplePipe : public beast::Pipe {
 public:
  SimplePipe(
      uint32_t max_candidates, uint32_t mem_size, uint32_t st_size, uint32_t sti_size)
    : Pipe(max_candidates), mem_size_{mem_size}, st_size_{st_size}, sti_size_{sti_size} {
  }

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) const override {
    beast::Program prg(program_data);
    beast::VmSession session(std::move(prg), mem_size_, st_size_, sti_size_);
    beast::CpuVirtualMachine virtual_machine;
    virtual_machine.setSilent(true);
    try {
      while (virtual_machine.step(session, true)) {
        // No action to perform, just statically step through the program.
      }
    } catch(...) {
      // If the program throws an exception, it gets 0.0 score.
      return 0.0;
    }

    beast::OperatorUsageEvaluator evaluator(beast::OpCode::NoOp);
    return evaluator.evaluate(session);
  }

 private:
  uint32_t mem_size_;

  uint32_t st_size_;

  uint32_t sti_size_;
};

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

  SimplePipe pipe(pop_size, mem_size, string_table_size, string_table_item_length);
  beast::RandomProgramFactory factory;

  while (pipe.hasSpace()) {
    beast::Program prg =
        factory.generate(prg_size, mem_size, string_table_size, string_table_item_length);
    pipe.addInput(prg.getData());
  }

  pipe.evolve();
  // TODO(fairlight1337): Process output.

  return 0;
}
