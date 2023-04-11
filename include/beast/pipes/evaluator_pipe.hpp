#ifndef BEAST_PIPES_EVALUATOR_PIPE_HPP_
#define BEAST_PIPES_EVALUATOR_PIPE_HPP_

// Internal
#include <beast/evaluators/aggregation_evaluator.hpp>
#include <beast/pipes/evolution_pipe.hpp>

namespace beast {

/**
 * @class EvaluatorPipe
 * @brief Evolves candidate programs according to attached evaluators
 *
 * This class represents a pipeline for evolving candidate programs using evaluators that are
 * attached to it.
 *
 * The basis for this pipe implementation is an `AggregationEvaluator` instance that any amount of
 * evaluators can be attached to (even other aggregators). Candidate programs are wrapped into a
 * proper `VmSession` object and are passed to the aggregator for scoring. The environmental
 * conditions (memory size, string table details) are passed as part of this evaluator's
 * constructor.
 *
 * The idea behind this evaluator is to abstract away the evaluation process form the pipeline
 * infrastructure. No special pipes need to be implemented for linearly evolving programs as long as
 * an evaluator combination can perform the same task using the EvaluatorPipe class.
 *
 * @author Jan Winkler
 * @date 2023-03-05
 */
class EvaluatorPipe : public EvolutionPipe {
 public:
  /**
   * @brief Initializes this EvaluatorPipe instance
   *
   * @param max_candidates The input/output size (number of candidate programs) of this pipe
   * @param variable_count The variable memory size for program evaluation
   * @param string_table_count The number of allowed string table entries
   * @param max_string_size The maximum allowed length of string table entries
   */
  EvaluatorPipe(uint32_t max_candidates, size_t variable_count, size_t string_table_count,
                size_t max_string_size);

  /**
   * @brief Attaches an evaluator to this pipe
   *
   * Programs that are evolved through this pipe are subject to scoring by any attached evaluator
   * instances. This function allows to attach arbitrary types of evaluators to it.
   *
   * @param evaluator The evaluator instance to attach to this pipe
   * @param weight The relative aggregator weight to apply to this evaluator's score
   * @param invert_logic Whether to invert this evaluator's scoring logic in the aggregator
   */
  void addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight, bool invert_logic);

  /**
   * @brief Evaluates a program and returns its score
   *
   * This function evaluates a program and returns its score based on the attached evaluators.
   *
   * @param program_data The program to evaluate
   * @return The score of the program
   */
  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override;

  /**
   * @brief Returns the size of variable memory used for evaluation
   *
   * @return The size of variable memory used for evaluation
   */
  uint32_t getMemorySize() const;

  /**
   * @brief Returns the allowed number of string table entries during evaluation
   *
   * @return The allowed number of string table entries during evaluation
   */
  uint32_t getStringTableSize() const;

  /**
   * @brief Returns the maximum length of any string table item
   *
   * @return The maximum length of any string table item
   */
  uint32_t getStringTableItemLength() const;

  /**
   * @brief Returns the list of attached evaluators with their respective weights and inversion
   * status
   *
   * @return The list of attached evaluators with their respective weights and inversion status
   */
  const std::vector<AggregationEvaluator::EvaluatorDescription>& getEvaluators() const;

 private:
  /**
   * @var EvaluatorPipe::variable_count_
   * @brief The size of variable memory used for evaluation
   */
  size_t variable_count_;

  /**
   * @var EvaluatorPipe::string_table_count_
   * @brief The allows number of string table entries during evaluation
   */
  size_t string_table_count_;

  /**
   * @var EvaluatorPipe::max_string_size
   * @brief The maximum length of any string table item
   */
  size_t max_string_size_;

  /**
   * @var EvaluatorPipe::evaluator_
   * @brief The base AggregationEvaluator other evaluators are attached to
   */
  AggregationEvaluator evaluator_;
};

} // namespace beast

#endif // BEAST_PIPES_EVALUATOR_PIPE_HPP_
