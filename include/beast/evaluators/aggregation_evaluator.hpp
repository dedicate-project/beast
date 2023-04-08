#ifndef BEAST_AGGREGATION_EVALUATOR_HPP_
#define BEAST_AGGREGATION_EVALUATOR_HPP_

// Standard
#include <memory>
#include <vector>

// Internal
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class Aggregation Evaluator
 * @brief Evaluator aggregating the results of any contained evaluators
 *
 * This evaluator is used to calculate aggregated score of one or more arbitrary evaluators that are
 * contained within it.
 *
 * @author Jan Winkler
 * @date 2023-01-26
 */
class AggregationEvaluator : public Evaluator {
 public:
  /**
   * @brief Describes an evaluator instance with weight and inversion flag
   */
  struct EvaluatorDescription {
    const std::shared_ptr<Evaluator> evaluator; ///< The evaluator to use for scoring
    double weight;                              ///< The weight to use for this evaluator's score
    bool invert_logic;                          ///< Whether to invert the scoring logic
  };

  /**
   * @fn AggregationEvaluator::addEvaluator
   * @brief Adds an evaluator to this aggregation evaluator
   *
   * Adds an evaluator to the aggregated list of evaluators. It can be weighted (relative; if you
   * want the same weight for all evaluators contained, give them all the same weight, e.g. 1.0) and
   * optionally its logic can be inverted (1.0 becomes 0.0 and vice versa, used to invert the
   * specific evaluator's effect). Weights may not be negative, and `nullptr` is not accepted as an
   * evaluator pointer.
   *
   * @param evaluator A pointer to the evaluator to add
   * @param weight The relative weight this evaluator's score should have in the aggregated score
   * @param invert_logic Whether to invert this evaluator's score before aggregating it
   */
  void addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight, bool invert_logic);

  /**
   * @fn AggregationEvaluator::evaluate
   * @brief Determines the aggregated score of all contained evaluators
   *
   * For each evaluator added, a weight and a logic inversion flag is set. When evaluating a
   * session, this evaluator will iterate through all contained evaluators and add up their weighted
   * scores. The respective evaluator weights are considered relative weights, so before the
   * evaluators are iterated, the total sum of weights is determined, and each evaluator's score
   * will be multiplied by `own weight / total weight`. In addition to that, if the respective
   * evaluator has the logic inversion flag set, its score will be augmented with `1.0 - score`
   * rather than `score`.
   *
   * If no evaluators were added to this aggregation evaluator when calling this function, an
   * exception is thrown.
   *
   * @param session The session object to base the score determination on
   * @return A score value from 0.0 to 1.0
   */
  double evaluate(const VmSession& session) override;

  const std::vector<EvaluatorDescription>& getEvaluators() const;

 private:
  /**
   * @var Aggregation::evaluators_
   * @brief Holds the evaluators contained in this aggregation evaluator
   */
  std::vector<EvaluatorDescription> evaluators_;
};

} // namespace beast

#endif // BEAST_AGGREGATION_EVALUATOR_HPP_
