#include <beast/evaluators/noop_evaluator.hpp>

namespace beast {

double NoOpEvaluator::evaluate(const VmSession& session) const {
  VmSession::RuntimeStatistics statistics = session.getRuntimeStatistics();

  // If no steps were executed, short-circuit.
  if (statistics.steps_executed == 0) {
    return 0.0;
  }

  // score = no-op operator executions / total operator executions
  return
      static_cast<double>(statistics.operator_executions[OpCode::NoOp]) /
      static_cast<double>(statistics.steps_executed);
}

}  // namespace beast
