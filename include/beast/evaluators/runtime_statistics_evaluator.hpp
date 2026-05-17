#ifndef BEAST_RUNTIME_STATISTICS_EVALUATOR_HPP_
#define BEAST_RUNTIME_STATISTICS_EVALUATOR_HPP_

// Internal
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Runtime Statistics Evaluator
 * @brief Evaluator determining a efficiency rating based on runtime statistics
 *
 * Calculates a nominal quality measure of how good the runtime behavior and static structure of a
 * program is.
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
   * The two weights `dyn_noop_weight` and `stat_noop_weight` decide on the emphasize this evaluator
   * puts on either the dynamic runtime behavior of a program, or its static structure. Their sum
   * may not exceed 1.0, and any delta between their sum and 1.0 will be used to put emphasize on
   * how much of the program was actually executed.
   *
   * @param dyn_noop_weight How much emphasize to put on dynamic runtime behavior (0.0 - 1.0)
   * @param stat_noop_weight How much emphasize to put on static program structure (0.0 - 1.0)
   */
  explicit RuntimeStatisticsEvaluator(double dyn_noop_weight, double stat_noop_weight);

  /**
   * @brief Destructor added for vtable consistency
   */
  ~RuntimeStatisticsEvaluator() override = default;

  /**
   * @fn OperatorUsageEvaluator::evaluate
   * @brief Evaluates the dynamic runtime behavior and the static structure of a session and program
   *
   * Depending on the score weights this evaluator was initialized with (`dyn_noop_weight` and
   * `stat_noop_weight`), this evaluation lays more emphasize on the dynamic runtime behavior or the
   * static structure.
   *
   * @param session The session object (and associated program) to evaluate
   * @return A score value from 0.0 (bad runtime behavior and/or static structure) to 1.0 (very good
   *         behavior and structure)
   */
  [[nodiscard]] double evaluate(const VmSession& session) override;

 private:
  /**
   * @var RuntimeStatisticsEvaluator::dyn_noop_weight_
   * @brief Denotes how much emphasize to put on dynamic runtime behavior
   */
  double dyn_noop_weight_;

  /**
   * @var RuntimeStatisticsEvaluator::stat_noop_weight_
   * @brief Denotes how much emphasize to put on static program structure
   */
  double stat_noop_weight_;

  /**
   * @var RuntimeStatisticsEvaluator::prg_exec_weight_
   * @brief Denotes how much emphasize to put on how much of the program was actually executed
   */
  double prg_exec_weight_;
};

} // namespace beast

#endif // BEAST_RUNTIME_STATISTICS_EVALUATOR_HPP_
