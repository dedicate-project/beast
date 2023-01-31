#include <beast/evaluators/runtime_statistics_evaluator.hpp>

#include <beast/cpu_virtual_machine.hpp>

namespace beast {

RuntimeStatisticsEvaluator::RuntimeStatisticsEvaluator(double w1, double w2)
  : w1_{w1}, w2_{w2}, w3_{1.0 - w1 - w2} {
  if (w1 + w2 > 1.0) {
    throw std::invalid_argument("w1 + w2 must be <= 1.0");
  }
}

double RuntimeStatisticsEvaluator::evaluate(const VmSession& session) const {
  VmSession::RuntimeStatistics dynamic_statistics = session.getRuntimeStatistics();

  /* The number of steps actually taken when executing the program. */
  const int32_t steps_executed = dynamic_statistics.steps_executed;
  /* The number of noop operators actually executed when executing the program. */
  const int32_t steps_executed_noop = dynamic_statistics.operator_executions[OpCode::NoOp];
  /* The fraction of noop vs. all steps executed. We want this to be low. */
  const double steps_executed_noop_fraction =
      static_cast<double>(steps_executed_noop) / static_cast<double>(steps_executed);

  VmSession static_session = session;
  static_session.reset();
  CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(static_session, true)) {}
  VmSession::RuntimeStatistics static_statistics = static_session.getRuntimeStatistics();

  /* The number of operators present in the program. */
  const int32_t total_steps = static_statistics.steps_executed;
  /* The number of noop operators present in the program. */
  const int32_t total_steps_noop = static_statistics.operator_executions[OpCode::NoOp];
  /* The fraction of noop vs. all steps present in the program. We want this to be high. */
  const double total_steps_noop_fraction =
      static_cast<double>(total_steps_noop) / static_cast<double>(total_steps);

  /* The fraction of individual static operators actually executed vs. actually present. Here, every
     statically present operator is only counted once. We want this to be high. */
  const double program_executed_fraction =
      static_cast<double>(dynamic_statistics.executed_indices.size()) /
      static_cast<double>(static_statistics.executed_indices.size());

  /*
    In order to get to a good measure of efficiency in both, static program structure AND dynamic
    runtime behavior, three things must be met:

    * As few operators executed as possible should be noop operators. This means that we don't waste
      time on empty cycles but execute efficiently. The variable holding a measure of this is
      steps_executed_noop_fraction.

    * As many operators actually present in the program code as possible should be noop
      operators. This means that the program's function can be executed with a slender static
      structure rather than bloated code. The variable holding a measure of this is
      total_steps_noop_fraction.

    * As few individual operators as possible should actually be executed. This means that a program
      makes good use of iterations and jumps, and doesn't rely on overfitting to any particular
      task. The variable holding a measure of this is program_executed_fraction.

    These aspects need to be combined into a single score value. Given the considerations from
    above, the formula for this score value hence becomes:

      w3 = 1.0 - w1 - w2, with w1 + w2 <= 1.0

      score =
          w1 * (1.0 - steps_executed_noop_fraction) +
          w2 * total_steps_noop_fraction +
          w3 * (1.0 - program_executed_fraction)

     The weight values w1 and w2 must be chosen carefully so that programs can correctly
     converge to a good balance between runtime and static structure efficiency.
   */

  return
      w1_ * (1.0 - steps_executed_noop_fraction) +
      w2_ * total_steps_noop_fraction +
      w3_ * (1.0 - program_executed_fraction);
}

}  // namespace beast
