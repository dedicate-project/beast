#include <beast/evaluators/random_serial_data_passthrough_evaluator.hpp>

// Standard
#include <limits>
#include <random>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

RandomSerialDataPassthroughEvaluator::RandomSerialDataPassthroughEvaluator(
    uint32_t data_count, uint32_t repeats, uint32_t max_steps)
  : data_count_{data_count}, repeats_{repeats}, max_steps_{max_steps} {
}

double RandomSerialDataPassthroughEvaluator::evaluate(const VmSession& session) {
  std::random_device random_device;
  std::mt19937 mersenne_twister(random_device());
  std::uniform_int_distribution<> distribution(
      std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());

  double worst_result = 1.0;
  for (uint32_t repeat = 0; repeat < repeats_; ++repeat) {
    std::vector<int32_t> values;
    for (uint32_t idx = 0; idx < data_count_; ++idx) {
      values.push_back(distribution(mersenne_twister));
    }

    // Make a copy because we're changing the state of the session.
    VmSession work_session = session;
    uint32_t correct_forwards = 0;

    work_session.setVariableBehavior(0, beast::VmSession::VariableIoBehavior::Input);
    work_session.setVariableBehavior(1, beast::VmSession::VariableIoBehavior::Output);

    work_session.setVariableValue(0, true, values[0]);

    try {
      correct_forwards = runProgram(work_session, values);
    } catch(...) {
      return 0.0;
    }

    const double result =
        std::min(1.0,
                 (static_cast<double>(correct_forwards) / static_cast<double>(values.size()))
                 + (correct_forwards == 0 ? static_cast<double>(values.size()) * 0.1 : 0.0));
    worst_result = std::min(worst_result, result);
  }

  return worst_result;
}

uint32_t RandomSerialDataPassthroughEvaluator::runProgram(
    VmSession& work_session, const std::vector<int32_t>& values) const {
  beast::CpuVirtualMachine virtual_machine;
  virtual_machine.setSilent(true);

  uint32_t value_index = 0;
  uint32_t step_count = 0;
  uint32_t correct_forwards = 0;

  do {
    if (work_session.hasOutputDataAvailable(1, true)) {
      if (work_session.getVariableValue(1, true) == values[value_index]) {
        correct_forwards++;
      }
      value_index++;
      if (value_index < values.size()) {
        work_session.setVariableValue(0, true, values[value_index]);
      }
    }
    step_count++;
  } while(value_index < values.size() &&
          step_count < max_steps_ &&
          virtual_machine.step(work_session, false));

  return correct_forwards;
}

}  // namespace beast
