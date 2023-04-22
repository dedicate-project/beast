#include <beast/evaluators/maze_evaluator.hpp>

// Standard
#include <limits>
#include <random>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

namespace {
maze::Maze getSolvableMaze(uint32_t rows, uint32_t cols, double difficulty, uint32_t max_tries) {
  uint32_t tries = 0;

  do {
    maze::Maze maze(rows, cols, difficulty);
    if (maze.isSolvable()) {
      return maze;
    }
    tries++;
  } while (tries < max_tries);

  throw std::runtime_error(
      std::string("Failed to generate a solvable maze with these parameters: ") +
      "rows=" + std::to_string(rows) + ", cols=" + std::to_string(cols) +
      ", difficulty=" + std::to_string(difficulty) + ", max_tries=" + std::to_string(max_tries));
}
} // namespace

MazeEvaluator::MazeEvaluator(uint32_t rows, uint32_t cols, double difficulty, uint32_t max_steps)
    : rows_{rows}, cols_{cols}, difficulty_{difficulty}, max_steps_{max_steps} {}

double MazeEvaluator::evaluate(const VmSession& session) {
  maze::Maze target_maze = getSolvableMaze(rows_, cols_, difficulty_, 10);

  VmSession local_session = session;
  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);
  const uint32_t max_steps = 1000;
  uint32_t current_steps = 0;
  while (virtual_machine.step(local_session, false)) {
    // TODO(fairlight1337): Implement this evaluation function: Feed current sight and player state
    // into maze, and get move commands out.
    static_cast<void>(target_maze);

    current_steps++;
    if (current_steps > max_steps) {
      return 0.0;
    }
  }

  double score = 0.0;
  return score;
}

uint32_t MazeEvaluator::getRows() const { return rows_; }

uint32_t MazeEvaluator::getCols() const { return cols_; }

double MazeEvaluator::getDifficulty() const { return difficulty_; }

uint32_t MazeEvaluator::getMaxSteps() const { return max_steps_; }

} // namespace beast
