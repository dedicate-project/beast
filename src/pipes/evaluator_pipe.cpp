#include <beast/pipes/evaluator_pipe.hpp>

namespace beast {

EvaluatorPipe::EvaluatorPipe(uint32_t max_candidates, size_t variable_count,
                             size_t string_table_count, size_t max_string_size)
    : EvolutionPipe(max_candidates), variable_count_{variable_count},
      string_table_count_{string_table_count}, max_string_size_{max_string_size} {}

void EvaluatorPipe::addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight,
                                 bool invert_logic) {
  evaluator_.addEvaluator(evaluator, weight, invert_logic);
}

double EvaluatorPipe::evaluate(const std::vector<unsigned char>& program_data) {
  VmSession session(Program(program_data), variable_count_, string_table_count_, max_string_size_);
  return evaluator_.evaluate(session);
}

uint32_t EvaluatorPipe::getMemorySize() const { return static_cast<uint32_t>(variable_count_); }

uint32_t EvaluatorPipe::getStringTableSize() const {
  return static_cast<uint32_t>(string_table_count_);
}

uint32_t EvaluatorPipe::getStringTableItemLength() const {
  return static_cast<uint32_t>(max_string_size_);
}

const std::vector<AggregationEvaluator::EvaluatorDescription>&
EvaluatorPipe::getEvaluators() const {
  return evaluator_.getEvaluators();
}

} // namespace beast
