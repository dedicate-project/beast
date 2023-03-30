#include <beast/evaluators/maze_evaluator.hpp>

// Standard
#include <limits>
#include <random>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

MazeEvaluator::MazeEvaluator(
    uint32_t rows, uint32_t cols, float difficulty, uint32_t max_steps)
  : rows_{rows}, cols_{cols}, difficulty_{difficulty}, max_steps_{max_steps} {
}

double MazeEvaluator::evaluate(const VmSession& /*session*/) {
  // TODO(fairlight1337): Implement this evaluation function.

  return 0.0;
}

}  // namespace beast
