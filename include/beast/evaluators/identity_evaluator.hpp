#ifndef BEAST_EVALUATORS_IDENTITY_EVALUATOR_HPP_
#define BEAST_EVALUATORS_IDENTITY_EVALUATOR_HPP_

// Standard
#include <cstdint>
#include <vector>

// BEAST
#include <beast/evaluators/bit_distance_evaluator.hpp>

namespace beast {

/**
 * @class IdentityEvaluator
 * @brief Scores a program on copying N input words verbatim to N output words
 *
 * The trivial first stage of the SHA-256 curriculum (or any curriculum that wants to
 * teach the GA to address every output slot before throwing harder transformations at
 * it). The reference function is the identity: `output[i] = input[i]`. A population
 * that solves this is one that's discovered the BEAST opcodes for reading inputs and
 * writing outputs and the variable indices the host pipe is wiring them to.
 *
 * Used as a seeding source for downstream curriculum stages: a `ProgramStorageSink`
 * on the identity pipeline's output gives a ledger of programs that "know how to
 * write to every output", which a `ProgramStorageSource` can then feed into a
 * `MultiplexerPipe` alongside fresh exploration in the next stage of the curriculum.
 */
class IdentityEvaluator : public BitDistanceEvaluator {
 public:
  IdentityEvaluator(uint32_t trial_count, uint32_t width, uint32_t max_steps_per_trial);

  [[nodiscard]] uint32_t getWidth() const noexcept;

 protected:
  [[nodiscard]] uint32_t inputCount() const override;
  [[nodiscard]] uint32_t outputCount() const override;
  void computeExpected(const std::vector<uint32_t>& inputs, std::vector<uint32_t>& outputs,
                       uint32_t trial_id) override;

 private:
  const uint32_t width_;
};

} // namespace beast

#endif // BEAST_EVALUATORS_IDENTITY_EVALUATOR_HPP_
