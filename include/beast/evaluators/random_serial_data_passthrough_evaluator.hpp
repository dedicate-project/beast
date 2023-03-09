#ifndef BEAST_EVALUATORS_RANDOM_SERIAL_DATA_PASSTHROUGH_EVALUATOR_HPP_
#define BEAST_EVALUATORS_RANDOM_SERIAL_DATA_PASSTHROUGH_EVALUATOR_HPP_

#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Random Serial Data Passthrough Evaluator
 * @brief Evaluates the ability of a program to serially pass through data
 *
 * This evaluator expects a program to be able to accept variable data on an input variable, and
 * pass it out on an output variable. The number of random data points that the program is tested
 * against, the number of repetitions, and the maximum number of steps any program is allowed to
 * spend on this task are parameters to this evaluator.
 *
 * When a program manages to pass through all data correctly, it reaches a score of 1.0. Any missed
 * data point deducts a percentage from that score, relative to the absolute number of data points
 * used. The lowest score reached over all repetitions is used as the final score. Throwing an
 * exception at any point causes a program to get a score of 0.0.
 *
 * @author Jan Winkler
 * @date 2023-03-04
 */
class RandomSerialDataPassthroughEvaluator : public Evaluator {
 public:
  /**
   * @fn RandomSerialDataPassthroughEvaluator::RandomSerialDataPassthroughEvaluator
   * @brief Initializes this evaluator instance
   *
   * @param data_count The number of random data points to use for evaluation
   * @param repeats The number of repetitions to perform
   * @param max_steps The total number of steps programs are allows during evaluation
   */
  explicit RandomSerialDataPassthroughEvaluator(
      uint32_t data_count, uint32_t repeats, uint32_t max_steps);

  /**
   * @fn RandomSerialDataPassthroughEvaluator::evaluate
   * @brief Evaluates the program for whether it can serially pass through data
   *
   * An amount of `data_points_` random `int32_t` values are generated that are serially passed to
   * the program through an input variable at index 0. The program is expected to pass through
   * (e.g., copy) the respective values into an output variable at index 1. Programs are scored
   * against the number of passthroughs they correctly perform. This procedure is performed
   * `repeats_` times. If programs exceed `max_steps_` runtime steps, they receive the score they
   * have finished up to that point.
   *
   * @param session The program session to evaluate
   * @return The final score resulting from the program evaluation
   */
  double evaluate(const VmSession& session) override;

 private:
  /**
   * @fn RandomSerialDataPassthroughEvaluator::runProgram
   * @brief Runs a candidate program session
   *
   * @param work_session The candidate program session to run
   * @param values The input values to provide to the program for forwarding
   * @return The number of correct input forwards
   */
  [[nodiscard]] uint32_t runProgram(
      VmSession& work_session, const std::vector<int32_t>& values) const;

  /**
   * @var RandomSerialDataPassthroughEvaluator::data_count_
   * @brief The number of random data points to use during evaluation
   */
  const uint32_t data_count_;

  /**
   * @var RandomSerialDataPassthroughEvaluator::repeats_
   * @brief The number of repetitions to perform during evaluation
   */
  const uint32_t repeats_;

  /**
   * @var RandomSerialDataPassthroughEvaluator::max_steps_
   * @brief The total number of steps programs are allowed during evaluation
   */
  const uint32_t max_steps_;
};

}  // namespace beast

#endif  // BEAST_EVALUATORS_RANDOM_SERIAL_DATA_PASSTHROUGH_EVALUATOR_HPP_
