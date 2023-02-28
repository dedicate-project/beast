// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

class SingleStepInputForwardPipe : public beast::Pipe {
 public:
  SingleStepInputForwardPipe(uint32_t max_candidates, uint32_t mem_size)
    : Pipe(max_candidates), mem_size_{mem_size} {
  }

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override {
    if (program_data.empty()) {
      return 0.0;
    }

    std::vector<int32_t> values = {27, -3, 128, 4000, 0};

    beast::Program prg(program_data);
    beast::CpuVirtualMachine virtual_machine;
    virtual_machine.setSilent(true);

    uint32_t correct_forwards = 0;
    const uint32_t steps_limit = 100;
    for (int32_t value : values) {
      beast::VmSession session(prg, mem_size_, 0, 0);
      session.setVariableBehavior(0, beast::VmSession::VariableIoBehavior::Input);
      session.setVariableBehavior(1, beast::VmSession::VariableIoBehavior::Output);
      uint32_t steps = 0;

      try {
        session.setVariableValue(0, true, value);
        while (steps < steps_limit && virtual_machine.step(session, false)) {
          if (session.hasOutputDataAvailable(1, true)) {
            // The program is expected to only assign the correct input value to the output
            // variable. When the variable is set, the program execution is ended.
            steps = steps_limit;
            if (session.getVariableValue(1, true) == value) {
              correct_forwards++;
            }
          }
          steps++;
        }
      } catch(...) {
        // If the program throws an exception, return a 0.0 score.
        return 0.0;
      }

      if (correct_forwards == 0) {
        // First forward failed, program is deemed unfit.
        return 0.0;
      }
    }

    return
        std::min(1.0,
                 (static_cast<double>(correct_forwards) / static_cast<double>(values.size()))
                 + 0.1);
  }

 private:
  uint32_t mem_size_;
};

class ContinuousInputForwardPipe : public beast::Pipe {
 public:
  ContinuousInputForwardPipe(uint32_t max_candidates, uint32_t mem_size)
    : Pipe(max_candidates), mem_size_{mem_size} {
  }

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override {
    if (program_data.empty()) {
      return 0.0;
    }

    std::vector<int32_t> values = {5, 33, -909871, 0, 0, 234, 1, 2, -1, 125, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0};

    beast::Program prg(program_data);
    beast::CpuVirtualMachine virtual_machine;
    virtual_machine.setSilent(true);

    beast::VmSession session(std::move(prg), mem_size_, 0, 0);
    session.setVariableBehavior(0, beast::VmSession::VariableIoBehavior::Input);
    session.setVariableBehavior(1, beast::VmSession::VariableIoBehavior::Output);
    uint32_t input_index = 0;
    const uint32_t steps_limit = 2000;
    uint32_t steps = 0;
    uint32_t correct_forwards = 0;

    session.setVariableValue(0, true, values[input_index]);

    try {
      do {
        if (session.hasOutputDataAvailable(1, true)) {
          if (session.getVariableValue(1, true) == values[input_index]) {
            correct_forwards++;
          }
          input_index++;
          if (input_index < values.size()) {
            session.setVariableValue(0, true, values[input_index]);
          }
        }
        steps++;
      } while (input_index < values.size() &&
               steps < steps_limit &&
               virtual_machine.step(session, false));
    } catch(...) {
      return 0.0;
    }

    return
        std::min(1.0,
                 (static_cast<double>(correct_forwards) / static_cast<double>(values.size()))
                 + (correct_forwards == 0 ? 0.1 : 0.0));
  }

 private:
  uint32_t mem_size_;
};

std::vector<std::vector<unsigned char>> runPipe(
    const std::shared_ptr<beast::Pipe>& pipe, std::vector<std::vector<unsigned char>> init_pop) {
  for (uint32_t pop_idx = 0; pop_idx < init_pop.size() && pipe->hasSpace(); ++pop_idx) {
    pipe->addInput(init_pop[pop_idx]);
  }

  pipe->evolve();

  std::vector<std::vector<unsigned char>> finalists;
  while (pipe->hasOutput()) {
    const beast::Pipe::OutputItem item = pipe->drawOutput();
    finalists.push_back(item.data);
  }

  return finalists;
}

int main(int /*argc*/, char** /*argv*/) {
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version "
            << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "."
            << static_cast<uint32_t>(version[2]) << "." << std::endl;

  /*
    The sorter pipeline consists of n consecutive steps that ultimately evolve programs that can
    sort arrays of arbitrary lengths (as much as the available VM memory allows):

    1. Input forwarding, single step: A program is able to receive a numeric value on an input
       variable, and assigns this value to an output variable. The program ends either when the
       output variable was assigned the correct value, or after at most 100 steps (which indicates
       failure if the value was not assigned to the designated output variable at that point).

    2. Input forwarding, continuously: Within a maximum number of steps, whenever a new value was
       set on the input variable, it should be forwarded to the output variable. The candidate
       programs will be rated based on how many values they got right.
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
      std::shared_ptr<SingleStepInputForwardPipe> pipe0 =
          std::make_shared<SingleStepInputForwardPipe>(pop_size, mem_size);
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

    std::shared_ptr<ContinuousInputForwardPipe> pipe1 =
        std::make_shared<ContinuousInputForwardPipe>(pop_size, mem_size);
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
