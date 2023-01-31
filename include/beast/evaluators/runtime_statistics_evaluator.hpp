#ifndef BEAST_RUNTIME_STATISTICS_EVALUATOR_HPP_
#define BEAST_RUNTIME_STATISTICS_EVALUATOR_HPP_

#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Runtime Statistics Evaluator
 * @brief Evaluator determining a efficiency rating based on runtime statistics
 *
 * TODO(fairlight1337): Document this class.
 *
 * @author Jan Winkler
 * @date 2023-01-31
 */
class RuntimeStatisticsEvaluator : public Evaluator {
 public:
  /**
   * @fn RuntimeStatisticsEvaluator::RuntimeStatisticsEvaluator
   * @brief Constructs the evaluator
   *
   * TODO(fairlight1337): Document this function.
   */
  RuntimeStatisticsEvaluator(double w1, double w2, double w3);

  /**
   * @fn OperatorUsageEvaluator::evaluate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   * @return A score value from 0.0 to 1.0
   */
  double evaluate(const VmSession& session) const override;

 private:
  double w1_;

  double w2_;

  double w3_;
};

}  // namespace beast

#endif  // BEAST_RUNTIME_STATISTICS_EVALUATOR_HPP_
