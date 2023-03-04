#ifndef BEAST_PIPES_EVALUATOR_PIPE_HPP_
#define BEAST_PIPES_EVALUATOR_PIPE_HPP_

// BEAST
#include <beast/evaluators/aggregation_evaluator.hpp>
#include <beast/pipe.hpp>

namespace beast {

class EvaluatorPipe : public Pipe {
 public:
  EvaluatorPipe(
      uint32_t max_candidates, size_t variable_count, size_t string_table_count,
      size_t max_string_size);

  void addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight, bool invert_logic);

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override;

 private:
  size_t variable_count_;

  size_t string_table_count_;

  size_t max_string_size_;

  AggregationEvaluator evaluator_;
};
  
}  // namespace beast

#endif  // BEAST_PIPES_EVALUATOR_PIPE_HPP_
