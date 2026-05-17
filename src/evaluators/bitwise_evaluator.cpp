#include <beast/evaluators/bitwise_evaluator.hpp>

// Standard
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0xB175E0B5U;

} // namespace

BitwiseEvaluator::Operation BitwiseEvaluator::parseOperation(const std::string& name) {
  if (name == "xor") {
    return Operation::Xor;
  }
  if (name == "and") {
    return Operation::And;
  }
  if (name == "or") {
    return Operation::Or;
  }
  if (name == "not") {
    return Operation::Not;
  }
  throw std::invalid_argument("Unknown BitwiseEvaluator operation: " + name);
}

std::string BitwiseEvaluator::operationName(Operation op) {
  switch (op) {
    case Operation::Xor:
      return "xor";
    case Operation::And:
      return "and";
    case Operation::Or:
      return "or";
    case Operation::Not:
      return "not";
  }
  // Unreachable in practice; clang-tidy still wants a fallback path.
  return "xor";
}

BitwiseEvaluator::BitwiseEvaluator(uint32_t trial_count, Operation op,
                                   uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed), operation_(op) {}

BitwiseEvaluator::Operation BitwiseEvaluator::getOperation() const noexcept { return operation_; }

uint32_t BitwiseEvaluator::inputCount() const {
  // NOT is unary; the others are binary.
  return operation_ == Operation::Not ? 1U : 2U;
}

uint32_t BitwiseEvaluator::outputCount() const { return 1U; }

void BitwiseEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                       std::vector<uint32_t>& outputs, uint32_t /*trial_id*/) {
  switch (operation_) {
    case Operation::Xor:
      outputs.at(0) = inputs.at(0) ^ inputs.at(1);
      return;
    case Operation::And:
      outputs.at(0) = inputs.at(0) & inputs.at(1);
      return;
    case Operation::Or:
      outputs.at(0) = inputs.at(0) | inputs.at(1);
      return;
    case Operation::Not:
      outputs.at(0) = ~inputs.at(0);
      return;
  }
}

} // namespace beast
