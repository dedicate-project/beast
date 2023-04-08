#include <beast/evaluators/aggregation_evaluator.hpp>

// Standard
#include <stdexcept>

namespace beast {

void AggregationEvaluator::addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight,
                                        bool invert_logic) {
  if (evaluator == nullptr) {
    throw std::invalid_argument("Null operator not allowed");
  }

  // Weights < 0.0 don't make sense in this context.
  if (weight < 0.0) {
    throw std::invalid_argument("Evaluator weight must be >= 0.0");
  }

  evaluators_.push_back({evaluator, weight, invert_logic});
}

double AggregationEvaluator::evaluate(const VmSession& session) {
  // If no evaluators are available, throw an exception.
  if (evaluators_.empty()) {
    throw std::invalid_argument("No evaluators defined prior to calling evaluate().");
  }

  // Sum up the total weight
  double total_weight = 0.0;
  for (const EvaluatorDescription& description : evaluators_) {
    total_weight += description.weight;
  }

  // Perform the scoring aggregation
  double total_score = 0.0;
  for (const EvaluatorDescription& description : evaluators_) {
    const double score = description.evaluator->evaluate(session);
    const double weight = description.weight / total_weight;
    total_score += weight * (description.invert_logic ? 1.0 - score : score);
  }

  return total_score;
}

const std::vector<AggregationEvaluator::EvaluatorDescription>&
AggregationEvaluator::getEvaluators() const {
  return evaluators_;
}

} // namespace beast
