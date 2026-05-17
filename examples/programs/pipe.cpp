// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

class SimplePipe : public beast::EvolutionPipe {
 public:
  SimplePipe(uint32_t max_candidates, uint32_t mem_size, uint32_t st_size, uint32_t sti_size)
      : EvolutionPipe(max_candidates), mem_size_{mem_size}, st_size_{st_size}, sti_size_{sti_size} {
  }

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override {
    if (program_data.empty()) {
      return 0.0;
    }
    beast::Program prg(program_data);
    beast::VmSession session(std::move(prg), mem_size_, st_size_, sti_size_);
    beast::CpuVirtualMachine virtual_machine;
    virtual_machine.setSilent(true);
    try {
      while (virtual_machine.step(session, true)) {
        // No action to perform, just statically step through the program.
      }
    } catch (...) {
      // If the program throws an exception, it gets a 0.0 score.
      return 0.0;
    }

    beast::OperatorUsageEvaluator evaluator(beast::OpCode::NoOp);
    return 1.0 - evaluator.evaluate(session);
  }

 private:
  uint32_t mem_size_;

  uint32_t st_size_;

  uint32_t sti_size_;
};

int main(int /*argc*/, char** /*argv*/) {
  /* Print BEAST library version. */
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version " << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "." << static_cast<uint32_t>(version[2]) << "."
            << std::endl;

  // Evolution parameters
  const uint32_t pop_size = 10;
  const uint32_t indiv_size = 100;

  // Program execution environment
  const uint32_t prg_size = 50;
  const uint32_t mem_size = 100;
  const uint32_t string_table_size = 10;
  const uint32_t string_table_item_length = 25;

  SimplePipe pipe(pop_size, mem_size, string_table_size, string_table_item_length);
  // pipe.setCutOffScore(0.9);
  beast::RandomProgramFactory factory;

  while (pipe.inputHasSpace(0)) {
    beast::Program prg =
        factory.generate(prg_size, mem_size, string_table_size, string_table_item_length);
    pipe.addInput(0, prg.getData());
  }

  pipe.execute();

  std::vector<std::vector<unsigned char>> finalists;
  while (pipe.hasOutput(0)) {
    const beast::Pipe::OutputItem item = pipe.drawOutput(0);
    finalists.push_back(item.data);
    std::cout << "Finalist: size = " << item.data.size() << " bytes, score = " << item.score
              << std::endl;
  }

  std::cout << "Got " << finalists.size() << " finalists." << std::endl;

  return 0;
}
