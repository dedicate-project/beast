#ifndef BEAST_NOOP_EVALUATOR_HPP_
#define BEAST_NOOP_EVALUATOR_HPP_

#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class NoOp Evaluator
 * @brief Evaluator determining the amount of no-op operations executed in a program
 *
 * This evaluator is used to calculate the amount of no-op operators executed in a program compared
 * to the overall number of operators executed.
 *
 * @author Jan Winkler
 * @date 2023-01-26
 */
class NoOpEvaluator : public Evaluator {
 public:
  /**
   * @fn NoOpEvaluator::evaluate
   * @brief Determines the portion of no-op operators during execution
   *
   * Evaluation formula: `score = no-op operations / total operations`
   * If no operations were performed at all, 0.0 is returned.
   *
   * @param session The session object to base the score determination on
   * @return A score value from 0.0 (no no-ops) to 1.0 (only no-ops)
   */
  double evaluate(const VmSession& session) override;
};

}  // namespace beast

#endif  // BEAST_NOOP_EVALUATOR_HPP_
