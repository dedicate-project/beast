// Standard
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

// BEAST
#include <beast/beast.hpp>

// End-to-end maze-evolution demo.
//
// Wires up:
//
//   ProgramFactoryPipe ---> EvaluatorPipe(MazeEvaluator) ---> NullSinkPipe
//                              (population of N candidates)
//
// The factory mints fresh random programs at the configured size; the EvaluatorPipe runs them
// against a freshly-generated maze on every evaluation; the sink drops any finalist programs
// that pass the cut-off score. We let the pipeline run for a few seconds and print summary
// metrics (executions, throughput per pipe) before stopping cleanly.
//
// The maze parameters and GA knobs below are intentionally on the lighter side so the demo
// finishes in seconds. Bump generations / population / maze difficulty for a more realistic
// search.

namespace {

// Bias the random-program generator toward instructions actually useful for maze navigation:
// reading the perception inputs, comparing them, setting the output move variable. Empty
// weights are still allowed (set them to 0 to disable an opcode entirely; missing entries
// default to 1.0).
beast::OpcodeWeights makeMazeOpcodeWeights() {
  beast::OpcodeWeights weights;
  // Reading/writing variables: load perception inputs, set the move output.
  weights[beast::OpCode::CopyVariable] = 6.0;
  weights[beast::OpCode::SetVariable] = 4.0;
  weights[beast::OpCode::SwapVariables] = 2.0;
  // Arithmetic + comparisons drive movement decisions based on perceived tile values.
  weights[beast::OpCode::AddConstantToVariable] = 2.0;
  weights[beast::OpCode::SubtractConstantFromVariable] = 2.0;
  weights[beast::OpCode::CompareIfVariableGtConstant] = 3.0;
  weights[beast::OpCode::CompareIfVariableLtConstant] = 3.0;
  weights[beast::OpCode::CompareIfVariableEqConstant] = 3.0;
  // Allow some control flow but keep it lightweight; full Turing power tends to spiral into
  // infinite loops at this stage.
  weights[beast::OpCode::RelativeJumpIfVariableGt0] = 1.0;
  weights[beast::OpCode::RelativeJumpIfVariableLt0] = 1.0;
  weights[beast::OpCode::RelativeJumpIfVariableEq0] = 1.0;
  weights[beast::OpCode::UnconditionalJumpToRelativeAddress] = 0.5;
  // Keep Terminate around but rare so it doesn't dominate slot 0 of the program.
  weights[beast::OpCode::Terminate] = 0.2;
  // Disable I/O probes, declares, string-table, stacks, and system calls for this demo.
  for (int code = static_cast<int>(beast::OpCode::CheckIfVariableIsInput);
       code < static_cast<int>(beast::OpCode::Size); ++code) {
    weights[static_cast<beast::OpCode>(code)] = 0.0;
  }
  // Re-enable LoadInputCountIntoVariable (it sits inside the I/O block) since it can be
  // useful for the program to know how many inputs are present.
  weights[beast::OpCode::LoadInputCountIntoVariable] = 0.5;
  return weights;
}

void printMetricsSnapshot(const beast::Pipeline::PipelineMetrics& metrics) {
  std::cout << "  pipes:\n";
  for (const auto& [name, pipe_metrics] : metrics.pipes) {
    std::cout << "    " << name << ": exec/s=" << pipe_metrics.execution_count;
    for (const auto& [slot, count] : pipe_metrics.outputs_sent) {
      std::cout << "  out[" << slot << "]/s=" << count;
    }
    for (const auto& [slot, count] : pipe_metrics.inputs_received) {
      std::cout << "  in[" << slot << "]/s=" << count;
    }
    std::cout << "\n";
  }
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
  using namespace std::chrono_literals;

  std::cout << "Using BEAST library version " << beast::getVersionString() << std::endl;

  // Maze + VM environment. The MazeEvaluator reserves (2*radius+1)^2 input variables for
  // perception tiles, plus 1 for food and 1 for the output move command. With radius=3 that's
  // 49+1+1 = 51, so we round up to 64 for slack.
  const uint32_t variable_count = 64;

  const uint32_t population = 16;
  const uint32_t max_program_size = 256;
  const uint32_t pipeline_buffer = 32;

  // Maze parameters. Keep small so each evaluation finishes quickly; the demo's point is to
  // show the wiring, not to actually solve hard mazes (that's what the maze_evaluator unit
  // test + a serious training run is for).
  const uint32_t maze_rows = 8;
  const uint32_t maze_cols = 8;
  const double maze_difficulty = 0.3;
  const uint32_t maze_max_steps = 4000;

  beast::Pipeline pipeline;

  auto factory = std::make_shared<beast::RandomProgramFactory>();
  auto factory_pipe = std::make_shared<beast::ProgramFactoryPipe>(
      /*max_candidates=*/population,
      /*max_size=*/max_program_size,
      /*memory_size=*/variable_count,
      /*string_table_size=*/0,
      /*string_table_item_length=*/0,
      factory);

  auto evaluator_pipe = std::make_shared<beast::EvaluatorPipe>(
      /*max_candidates=*/population,
      /*variable_count=*/variable_count,
      /*string_table_count=*/0,
      /*max_string_size=*/0);
  auto maze_evaluator = std::make_shared<beast::MazeEvaluator>(maze_rows, maze_cols,
                                                                maze_difficulty, maze_max_steps);
  evaluator_pipe->addEvaluator(maze_evaluator, /*weight=*/1.0, /*invert_logic=*/false);
  evaluator_pipe->setCutOffScore(0.6); // Keep only programs that beat ~60% of the ideal path.

  // Tune the GA for this demo: bias toward arithmetic + comparisons, cap genome growth at
  // 1 KB so evaluations stay fast, and keep generations modest.
  beast::EvolutionPipe::EvolutionParameters params;
  params.generations = 10;
  params.crossover_probability = 0.75;
  params.mutation_probability = 0.10;
  params.elitism = true;
  params.max_genome_bytes = 1024;
  params.variable_count = variable_count;
  params.starting_program_size = 64;
  params.opcode_weights = makeMazeOpcodeWeights();
  evaluator_pipe->setEvolutionParameters(params);

  auto sink_pipe = std::make_shared<beast::NullSinkPipe>(/*max_candidates=*/population);

  pipeline.addPipe("factory", factory_pipe);
  pipeline.addPipe("evaluator", evaluator_pipe);
  pipeline.addPipe("sink", sink_pipe);

  pipeline.connectPipes(factory_pipe, 0, evaluator_pipe, 0, pipeline_buffer);
  pipeline.connectPipes(evaluator_pipe, 0, sink_pipe, 0, pipeline_buffer);

  std::cout << "Starting maze-evolution pipeline; will run for ~5 seconds." << std::endl;
  pipeline.start();

  const auto start = std::chrono::steady_clock::now();
  const auto deadline = start + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(500ms);
    const auto snap = pipeline.getMetrics();
    std::cout << "[t+" << std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count()
              << "ms]" << std::endl;
    printMetricsSnapshot(snap);
  }

  std::cout << "Stopping pipeline..." << std::endl;
  pipeline.stop();
  std::cout << "Done." << std::endl;
  return EXIT_SUCCESS;
}
