#include <beast/evaluators/maze_evaluator.hpp>

// Standard
#include <algorithm>
#include <cmath>
#include <iostream>
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

double computeMetric(uint32_t ideal_moves, uint32_t moves) {
  if (moves <= ideal_moves) {
    return 1.0;
  }

  const double exponent =
      -5.0 * (static_cast<double>(moves) / static_cast<double>(ideal_moves) - 1.0);
  const double metric = 0.99 * std::exp(exponent) + 0.01;
  return std::clamp(metric, 0.01, 1.0);
}
} // namespace

MazeEvaluator::MazeEvaluator(uint32_t rows, uint32_t cols, double difficulty, uint32_t max_steps)
    : rows_{rows}, cols_{cols}, difficulty_{difficulty}, max_steps_{max_steps} {}

double MazeEvaluator::evaluate(const VmSession& session) {
  const uint32_t visibility_radius = 3;
  maze::Maze target_maze = getSolvableMaze(rows_, cols_, difficulty_, 10);

  VmSession local_session = session;
  // Set inputs for perceived tiles
  const uint32_t input_indices = (visibility_radius * 2 + 1) * (visibility_radius * 2 + 1);
  for (uint32_t input_index = 0; input_index < input_indices; ++input_index) {
    local_session.setVariableBehavior(input_index, beast::VmSession::VariableIoBehavior::Input);
  }
  // Set input for current player food
  const uint32_t food_input = input_indices;
  local_session.setVariableBehavior(food_input, beast::VmSession::VariableIoBehavior::Input);
  // Set output for player move
  const uint32_t move_output = input_indices + 1;
  local_session.setVariableBehavior(move_output, beast::VmSession::VariableIoBehavior::Output);

  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);
  const uint32_t max_steps = 10000;
  uint32_t current_steps = 0;
  uint32_t moves = 0;
  bool update_sight = true;
  const auto ideal_path = target_maze.solve();
  try {
    while (virtual_machine.step(local_session, false)) {
      if (local_session.hasOutputDataAvailable(move_output, true)) {
        // A move command was issued. Process it here.
        const auto move = local_session.getVariableValue(move_output, true);
        if (target_maze.movePlayer(static_cast<maze::Maze::Move>(move))) {
          // The player moved. Update perception data.
          moves++;
          update_sight = true;
        } else {
          // The player ran out of food.
          std::cout << "No food " << moves << std::endl;
          return 0.02;
        }
      }

      if (update_sight) {
        uint32_t index = 0;
        const auto perception = target_maze.perceiveTiles(visibility_radius);
        for (const auto& col : perception) {
          for (const auto& tile : col) {
            local_session.setVariableValue(index, true, static_cast<uint32_t>(tile));
            index++;
          }
        }
        local_session.setVariableValue(food_input, true, target_maze.getPlayerCurrentFood());
        update_sight = false;
      }

      if (target_maze.isFinished()) {
        // Score based on how many more moves we needed than A*.
        const double metric = computeMetric(ideal_path.size(), moves);
        std::cout << metric << std::endl;
        return metric;
      }

      current_steps++;
      if (current_steps > max_steps) {
        std::cout << "Clamp (" << moves << ")" << std::endl;
        return moves == 0 ? 0.0 : 0.01;
      }
    }
  } catch(...) {
    return 0.0;
  }

  return 0.0;
}

uint32_t MazeEvaluator::getRows() const { return rows_; }

uint32_t MazeEvaluator::getCols() const { return cols_; }

double MazeEvaluator::getDifficulty() const { return difficulty_; }

uint32_t MazeEvaluator::getMaxSteps() const { return max_steps_; }

} // namespace beast
