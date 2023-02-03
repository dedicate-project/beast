#include <beast/evaluators/runtime_statistics_evaluator.hpp>

// Standard
#include <stdexcept>

// Internal
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

RuntimeStatisticsEvaluator::RuntimeStatisticsEvaluator(
    double dyn_noop_weight, double stat_noop_weight)
  : dyn_noop_weight_{dyn_noop_weight}, stat_noop_weight_{stat_noop_weight},
    prg_exec_weight_{1.0 - dyn_noop_weight - stat_noop_weight} {
  if (dyn_noop_weight + stat_noop_weight > 1.0) {
    throw std::invalid_argument("dyn_noop_weight + stat_noop_weight must be <= 1.0");
  }

  if (dyn_noop_weight < 0.0 || stat_noop_weight < 0.0) {
    throw std::invalid_argument("dyn_noop_weight, stat_noop_weight must each be >= 0.0");
  }
}

double RuntimeStatisticsEvaluator::evaluate(const VmSession& session) const {
  VmSession::RuntimeStatistics dynamic_statistics = session.getRuntimeStatistics();

  /* The number of steps actually taken when executing the program. */
  const uint32_t steps_executed = dynamic_statistics.steps_executed;
  if (steps_executed == 0) {
    // Not executing steps results in a zero score.
    return 0.0;
  }
  /* The number of noop operators actually executed when executing the program. */
  const uint32_t steps_executed_noop = dynamic_statistics.operator_executions[OpCode::NoOp];
  /* The fraction of noop vs. all steps executed. We want this to be low. */
  const double steps_executed_noop_fraction =
      static_cast<double>(steps_executed_noop) / static_cast<double>(steps_executed);

  VmSession static_session = session;
  static_session.reset();
  CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);
  while (virtual_machine.step(static_session, true)) {
    /* Do nothing, just execute the entirely program in a static manner. */
  }
  VmSession::RuntimeStatistics static_statistics = static_session.getRuntimeStatistics();

  /* The number of operators present in the program. */
  const uint32_t total_steps = static_statistics.steps_executed;
  if (total_steps == 0) {
    // Not executing steps results in a zero score.
    return 0.0;
  }
  /* The number of noop operators present in the program. */
  const uint32_t total_steps_noop = static_statistics.operator_executions[OpCode::NoOp];
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

      prg_exec_weight = 1.0 - dyn_noop_weight - stat_noop_weight
                        with dyn_noop_weight + stat_noop_weight <= 1.0

      score =
          dyn_noop_weight * (1.0 - steps_executed_noop_fraction) +
          stat_noop_weight * total_steps_noop_fraction +
          prg_exec_weight * (1.0 - program_executed_fraction)

     The weight values dyn_noop_weight and stat_noop_weight must be chosen carefully so that
     programs can correctly converge to a good balance between runtime and static structure
     efficiency.
   */

  return
      dyn_noop_weight_ * (1.0 - steps_executed_noop_fraction) +
      stat_noop_weight_ * total_steps_noop_fraction +
      prg_exec_weight_ * (1.0 - program_executed_fraction);
}

}  // namespace beast
