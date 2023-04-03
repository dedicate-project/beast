// Standard
#include <chrono>

// Catch2
#include <catch2/catch.hpp>

// BEAST
#include <beast/beast.hpp>

namespace {
std::vector<std::vector<unsigned char>> runPipe(
    const std::shared_ptr<beast::Pipe>& pipe, std::vector<std::vector<unsigned char>> init_pop) {
  for (uint32_t pop_idx = 0; pop_idx < init_pop.size() && pipe->hasSpace(); ++pop_idx) {
    pipe->addInput(init_pop[pop_idx]);
  }

  pipe->execute();

  std::vector<std::vector<unsigned char>> finalists;
  while (pipe->hasOutput(0)) {
    const beast::Pipe::OutputItem item = pipe->drawOutput(0);
    finalists.push_back(item.data);
  }

  return finalists;
}
}  // namespace

TEST_CASE("random_serial_passthrough_generates_50_valid_programs_within_10s", "pipelines") {
  using namespace std::chrono_literals;
  const auto max_runtime = 10s;

  // Evolution parameters
  const uint32_t pop_size = 50;

  // Program execution environment
  const uint32_t prg_size = 200;
  const uint32_t mem_size = 3;

  const std::chrono::time_point<std::chrono::system_clock> start_time =
      std::chrono::system_clock::now();

  std::vector<std::vector<unsigned char>> staged;
  uint32_t last_staged = 0;
  while (staged.size() < pop_size) {
    const std::chrono::time_point<std::chrono::system_clock> current_time =
        std::chrono::system_clock::now();
    REQUIRE(current_time - start_time < max_runtime);

    std::shared_ptr<beast::EvaluatorPipe> pipe =
        std::make_shared<beast::EvaluatorPipe>(pop_size, mem_size, 0, 0);
    const auto eval = std::make_shared<beast::RandomSerialDataPassthroughEvaluator>(1, 5, 100);
    pipe->addEvaluator(eval, 1.0, false);
    pipe->setCutOffScore(1.0);

    std::vector<std::vector<unsigned char>> init_pop;
    beast::RandomProgramFactory factory;
    for (uint32_t pop_idx = 0; pop_idx < pop_size; ++pop_idx) {
      init_pop.push_back(factory.generate(prg_size, mem_size, 0, 0).getData());
    }

    std::vector<std::vector<unsigned char>> finalists = runPipe(pipe, init_pop);
    for (const auto& finalist : finalists) {
      staged.push_back(finalist);
    }
    if (staged.size() > last_staged) {
      last_staged = staged.size();
    }
  }
}
