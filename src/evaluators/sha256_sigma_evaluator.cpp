#include <beast/evaluators/sha256_sigma_evaluator.hpp>

// Standard
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace beast {

namespace {

constexpr uint32_t kRngSeed = 0x516A1234U;

constexpr uint32_t rotr(uint32_t value, uint32_t amount) noexcept {
  amount &= 31U;
  if (amount == 0U) {
    return value;
  }
  return (value >> amount) | (value << (32U - amount));
}

constexpr uint32_t bigSigma0(uint32_t x) noexcept {
  return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

constexpr uint32_t bigSigma1(uint32_t x) noexcept {
  return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

constexpr uint32_t smallSigma0(uint32_t x) noexcept {
  return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3U);
}

constexpr uint32_t smallSigma1(uint32_t x) noexcept {
  return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10U);
}

} // namespace

Sha256SigmaEvaluator::Variant Sha256SigmaEvaluator::parseVariant(const std::string& name) {
  if (name == "big0") {
    return Variant::BigSigma0;
  }
  if (name == "big1") {
    return Variant::BigSigma1;
  }
  if (name == "small0") {
    return Variant::SmallSigma0;
  }
  if (name == "small1") {
    return Variant::SmallSigma1;
  }
  throw std::invalid_argument("Unknown Sha256SigmaEvaluator variant: " + name);
}

std::string Sha256SigmaEvaluator::variantName(Variant variant) {
  switch (variant) {
    case Variant::BigSigma0:
      return "big0";
    case Variant::BigSigma1:
      return "big1";
    case Variant::SmallSigma0:
      return "small0";
    case Variant::SmallSigma1:
      return "small1";
  }
  return "big0";
}

Sha256SigmaEvaluator::Sha256SigmaEvaluator(uint32_t trial_count, Variant variant,
                                           uint32_t max_steps_per_trial)
    : BitDistanceEvaluator(trial_count, max_steps_per_trial, kRngSeed), variant_(variant) {}

Sha256SigmaEvaluator::Variant Sha256SigmaEvaluator::getVariant() const noexcept {
  return variant_;
}

uint32_t Sha256SigmaEvaluator::inputCount() const { return 1U; }

uint32_t Sha256SigmaEvaluator::outputCount() const { return 1U; }

void Sha256SigmaEvaluator::computeExpected(const std::vector<uint32_t>& inputs,
                                           std::vector<uint32_t>& outputs,
                                           uint32_t /*trial_id*/) {
  const uint32_t x = inputs.at(0);
  switch (variant_) {
    case Variant::BigSigma0:
      outputs.at(0) = bigSigma0(x);
      return;
    case Variant::BigSigma1:
      outputs.at(0) = bigSigma1(x);
      return;
    case Variant::SmallSigma0:
      outputs.at(0) = smallSigma0(x);
      return;
    case Variant::SmallSigma1:
      outputs.at(0) = smallSigma1(x);
      return;
  }
}

} // namespace beast
