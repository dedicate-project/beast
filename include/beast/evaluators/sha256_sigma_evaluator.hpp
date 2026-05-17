#ifndef BEAST_EVALUATORS_SHA256_SIGMA_EVALUATOR_HPP_
#define BEAST_EVALUATORS_SHA256_SIGMA_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <string>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class Sha256SigmaEvaluator
 * @brief Scores a program on computing one of the four SHA-256 sigma functions
 *
 * SHA-256 uses four single-input mixing functions: two "big sigma" functions inside
 * the round body, and two "small sigma" functions inside the message-schedule
 * expansion. They differ only in the rotation amounts and (for small sigma) one of
 * the three terms being a shift instead of a rotate.
 *
 * - BigSigma0(x)   = rotr(x, 2)  XOR rotr(x, 13) XOR rotr(x, 22)
 * - BigSigma1(x)   = rotr(x, 6)  XOR rotr(x, 11) XOR rotr(x, 25)
 * - SmallSigma0(x) = rotr(x, 7)  XOR rotr(x, 18) XOR  shr(x, 3)
 * - SmallSigma1(x) = rotr(x, 17) XOR rotr(x, 19) XOR  shr(x, 10)
 *
 * Each evaluator instance evolves a program for ONE of the four. They're separate
 * skills because the rotation/shift amounts differ; a population that nails BigSigma0
 * won't necessarily generalise to BigSigma1 without re-evolution. The four
 * implementations can run in parallel as four pipelines feeding into four ledgers.
 *
 * One input word (var 0) -> one output word (var inputCount()+1 = 2).
 */
class Sha256SigmaEvaluator : public BitDistanceEvaluator {
 public:
  enum class Variant : uint8_t { BigSigma0 = 0, BigSigma1 = 1, SmallSigma0 = 2, SmallSigma1 = 3 };

  static Variant parseVariant(const std::string& name);
  static std::string variantName(Variant variant);

  Sha256SigmaEvaluator(uint32_t trial_count, Variant variant, uint32_t max_steps_per_trial);

  [[nodiscard]] Variant getVariant() const noexcept;

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;

 private:
  const Variant variant_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_SHA256_SIGMA_EVALUATOR_HPP_
