#ifndef BEAST_RANDOM_SERIAL_DATA_PASSTHROUGH_EVALUATOR_HPP_
#define BEAST_RANDOM_SERIAL_DATA_PASSTHROUGH_EVALUATOR_HPP_

#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Random Serial Data Passthrough Evaluator
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this class.
 *
 * @author Jan Winkler
 * @date 2023-03-04
 */
class RandomSerialDataPassthroughEvaluator : public Evaluator {
 public:
  /**
   * @fn RandomSerialDataPassthroughEvaluator::RandomSerialDataPassthroughEvaluator
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  explicit RandomSerialDataPassthroughEvaluator(
      uint32_t data_count, uint32_t repeats, uint32_t max_steps);

  /**
   * @fn RandomSerialDataPassthroughEvaluator::evaluate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  double evaluate(const VmSession& session) override;

 private:
  const uint32_t data_count_;

  const uint32_t repeats_;

  const uint32_t max_steps_;
};

}  // namespace beast

#endif  // BEAST_RANDOM_SERIAL_DATA_PASSTHROUGH_EVALUATOR_HPP_
