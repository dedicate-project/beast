#ifndef BEAST_OPERATOR_USAGE_EVALUATOR_HPP_
#define BEAST_OPERATOR_USAGE_EVALUATOR_HPP_

#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Operator Usage Evaluator
 * @brief Evaluator determining the amount of specific operations executed in a program
 *
 * This evaluator is used to calculate the amount of specific operators executed in a program
 * compared to the overall number of operators executed. The type of operator to look for can be
 * specified during construction. The score returned by the evaluator represents the fraction this
 * operator type represents in the overall operators used in the program.
 *
 * @author Jan Winkler
 * @date 2023-01-26
 */
class OperatorUsageEvaluator : public Evaluator {
 public:
  /**
   * @fn OperatorUsageEvaluator::OperatorUsageEvaluator
   * @brief Constructs the evaluator with a specific operator type whose usage to count
   *
   * @param opcode The operator code to count
   */
  explicit OperatorUsageEvaluator(OpCode opcode);

  /**
   * @fn OperatorUsageEvaluator::evaluate
   * @brief Determines the portion of specific operators during execution
   *
   * Evaluation formula: `score = specific operations / total operations`
   * If no operations were performed at all, 0.0 is returned.
   *
   * @param session The session object to base the score determination on
   * @return A score value from 0.0 (specific op not found) to 1.0 (only specific op)
   */
  double evaluate(const VmSession& session) override;

 private:
  /**
   * @var OperatorUsageEvaluator::opcode_
   * @brief Denotes which operator's usage this evaluator counts
   */
  OpCode opcode_;
};

} // namespace beast

#endif // BEAST_OPERATOR_USAGE_EVALUATOR_HPP_
