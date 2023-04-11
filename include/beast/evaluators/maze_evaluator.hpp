#ifndef BEAST_EVALUATORS_MAZE_EVALUATOR_HPP_
#define BEAST_EVALUATORS_MAZE_EVALUATOR_HPP_

// Maze
#include <maze/maze.hpp>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class MazeEvaluator
 * @brief Evaluates the ability of a program to find a path through a maze.
 *
 * MazeEvaluator is a derived class of the Evaluator base class that specializes
 * in evaluating the performance of a given program to find a path through a maze.
 * The maze is represented by the custom Maze class, and the evaluation takes into
 * account various factors, such as maze size, difficulty, and the player's food
 * level during navigation.
 *
 * @author Jan Winkler
 * @date 2023-03-30
 */
class MazeEvaluator : public Evaluator {
 public:
  /**
   * @brief Initializes this evaluator instance.
   *
   * @param rows The number of rows used for the maze.
   * @param cols The number of columns used for the maze.
   * @param difficulty The difficulty used for the maze (0.0-1.0).
   * @param max_steps The total number of steps programs are allowed during evaluation.
   */
  explicit MazeEvaluator(uint32_t rows, uint32_t cols, double difficulty, uint32_t max_steps);

  /**
   * @brief Evaluates the program for whether it can find a path through a maze.
   *
   * This function evaluates the given session based on its ability to navigate through
   * a maze. The maze is created using the given rows, columns, and difficulty parameters.
   * The evaluation takes into account the program's ability to maintain a food level,
   * find food in the maze, and the total number of steps taken. The score is computed
   * based on these factors, with a higher score representing better performance.
   *
   * @param session The program session to evaluate.
   * @return The final score resulting from the program evaluation.
   */
  double evaluate(const VmSession& session) override;

  /**
   * @brief Returns the number of rows used for the maze.
   *
   * @return The number of rows used for the maze.
   */
  uint32_t getRows() const;

  /**
   * @brief Returns the number of columns used for the maze.
   *
   * @return The number of columns used for the maze.
   */
  uint32_t getCols() const;

  /**
   * @brief Returns the difficulty used for the maze.
   *
   * @return The difficulty used for the maze.
   */
  double getDifficulty() const;

  /**
   * @brief Returns the total number of steps programs are allowed during evaluation.
   *
   * @return The total number of steps programs are allowed during evaluation.
   */
  uint32_t getMaxSteps() const;

 private:
  /**
   * @var rows_
   * @brief The rows used for the maze.
   */
  const uint32_t rows_;

  /**
   * @var cols_
   * @brief The columns used for the maze.
   */
  const uint32_t cols_;

  /**
   * @var difficulty_
   * @brief The difficulty used for the maze.
   */
  const double difficulty_;

  /**
   * @var max_steps_
   * @brief The total number of steps programs are allowed during evaluation.
   */
  const uint32_t max_steps_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_MAZE_EVALUATOR_HPP_
