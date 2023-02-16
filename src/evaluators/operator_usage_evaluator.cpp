#include <beast/evaluators/operator_usage_evaluator.hpp>

namespace beast {

OperatorUsageEvaluator::OperatorUsageEvaluator(OpCode opcode)
  : opcode_{opcode} {
}

double OperatorUsageEvaluator::evaluate(const VmSession& session) {
  VmSession::RuntimeStatistics statistics = session.getRuntimeStatistics();

  // If no steps were executed, short-circuit.
  if (statistics.steps_executed == 0) {
    return 0.0;
  }

  // score = specific operator executions / total operator executions
  return
      static_cast<double>(statistics.operator_executions[opcode_]) /
      static_cast<double>(statistics.steps_executed);
}

}  // namespace beast
