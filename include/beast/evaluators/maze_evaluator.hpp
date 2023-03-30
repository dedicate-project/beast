#ifndef BEAST_EVALUATORS_MAZE_EVALUATOR_HPP_
#define BEAST_EVALUATORS_MAZE_EVALUATOR_HPP_

// Maze
#include <maze/maze.hpp>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Maze Evaluator
 * @brief Evaluates the ability of a program to find a path through a maze
 *
 * TODO(fairlight1337): Document this class.
 *
 * @author Jan Winkler
 * @date 2023-03-30
 */
class MazeEvaluator : public Evaluator {
 public:
  /**
   * @fn MazeEvaluator::MazeEvaluator
   * @brief Initializes this evaluator instance
   *
   * @param rows The number of rows used for the maze
   * @param cols The number of columns used for the maze
   * @param difficulty The difficulty used for the maze (0.0-1.0)
   * @param max_steps The total number of steps programs are allows during evaluation
   */
  explicit MazeEvaluator(
      uint32_t rows, uint32_t cols, float difficulty, uint32_t max_steps);

  /**
   * @fn MazeEvaluator::evaluate
   * @brief Evaluates the program for whether it can find a path through a maze
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param session The program session to evaluate
   * @return The final score resulting from the program evaluation
   */
  double evaluate(const VmSession& session) override;

 private:
  /**
   * @var MazeEvaluator::data_count_
   * @brief The rows used for the maze
   */
  const uint32_t rows_;

  /**
   * @var MazeEvaluator::repeats_
   * @brief The columns used or the maze
   */
  const uint32_t cols_;

  /**
   * @var MazeEvaluator::repeats_
   * @brief The difficulty used for the maze
   */
  const float difficulty_;

  /**
   * @var MazeEvaluator::max_steps_
   * @brief The total number of steps programs are allowed during evaluation
   */
  const uint32_t max_steps_;
};

}  // namespace beast

#endif  // BEAST_EVALUATORS_MAZE_EVALUATOR_HPP_
