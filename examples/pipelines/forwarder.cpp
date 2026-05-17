// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

std::vector<std::vector<unsigned char>>
runPipe(const std::shared_ptr<beast::Pipe>& pipe,
        const std::vector<std::vector<unsigned char>>& init_pop) {
  for (uint32_t pop_idx = 0; pop_idx < init_pop.size() && pipe->inputHasSpace(0); ++pop_idx) {
    pipe->addInput(0, init_pop[pop_idx]);
  }

  pipe->execute();

  std::vector<std::vector<unsigned char>> finalists;
  while (pipe->hasOutput(0)) {
    const beast::Pipe::OutputItem item = pipe->drawOutput(0);
    finalists.push_back(item.data);
  }

  return finalists;
}

int main(int /*argc*/, char** /*argv*/) {
  std::cout << "Using BEAST library version " << beast::getVersionString() << std::endl;

  /*
    The sorter pipeline consists of n consecutive steps that ultimately evolve
    programs that can sort arrays of arbitrary lengths (as much as the available
    VM memory allows):

    1. Input forwarding, single step: A program is able to receive a numeric
    value on an input variable, and assigns this value to an output variable.
    The program ends either when the output variable was assigned the correct
    value, or after at most 100 steps (which indicates failure if the value was
    not assigned to the designated output variable at that point).

    2. Input forwarding, continuously: Within a maximum number of steps,
    whenever a new value was set on the input variable, it should be forwarded
    to the output variable. The candidate programs will be rated based on how
    many values they got right.
   */

  // Evolution parameters
  const uint32_t pop_size = 50;

  // Program execution environment
  const uint32_t prg_size = 200;
  const uint32_t mem_size = 3;

  std::vector<std::vector<unsigned char>> staged1;
  uint32_t last_staged1 = 0;
  while (staged1.size() < pop_size) {
    std::vector<std::vector<unsigned char>> staged0;
    uint32_t last_staged0 = 0;
    while (staged0.size() < pop_size) {
      std::shared_ptr<beast::EvaluatorPipe> pipe0 =
          std::make_shared<beast::EvaluatorPipe>(pop_size, mem_size, 0, 0);
      const auto eval0 = std::make_shared<beast::RandomSerialDataPassthroughEvaluator>(1, 5, 100);
      pipe0->addEvaluator(eval0, 1.0, false);
      pipe0->setCutOffScore(1.0);

      std::vector<std::vector<unsigned char>> init_pop0;
      beast::RandomProgramFactory factory;
      for (uint32_t pop_idx = 0; pop_idx < pop_size; ++pop_idx) {
        init_pop0.push_back(factory.generate(prg_size, mem_size, 0, 0).getData());
      }

      std::vector<std::vector<unsigned char>> finalists0 = runPipe(pipe0, init_pop0);
      for (const auto& finalist : finalists0) {
        staged0.push_back(finalist);
      }
      if (staged0.size() > last_staged0) {
        std::cout << "staged0 = " << staged0.size() << std::endl;
        last_staged0 = staged0.size();
      }
    }

    std::shared_ptr<beast::EvaluatorPipe> pipe1 =
        std::make_shared<beast::EvaluatorPipe>(pop_size, mem_size, 0, 0);
    const auto eval1 = std::make_shared<beast::RandomSerialDataPassthroughEvaluator>(10, 2, 2000);
    pipe1->addEvaluator(eval1, 1.0, false);
    pipe1->setCutOffScore(1.0);

    std::vector<std::vector<unsigned char>> finalists1 = runPipe(pipe1, staged0);
    staged0.clear();
    for (const auto& finalist : finalists1) {
      staged1.push_back(finalist);
    }
    if (staged1.size() > last_staged1) {
      std::cout << " staged1 = " << staged1.size() << std::endl;
      last_staged1 = staged1.size();
    }
  }

  std::cout << "Finalists:" << std::endl;
  for (const auto& staged : staged1) {
    std::cout << "* Size = " << staged.size() << " bytes" << std::endl;
  }

  return EXIT_SUCCESS;
}
