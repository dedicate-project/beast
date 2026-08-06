#ifndef BEAST_PIPES_FILTER_PIPE_HPP_
#define BEAST_PIPES_FILTER_PIPE_HPP_

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class FilterPipe
 * @brief Routes candidates to one of two output slots based on a fitness threshold
 *
 * A single-input, two-output passthrough that inspects each candidate's score and routes
 * it to the appropriate output slot:
 *   - Output slot 0 ("below"): candidates with `score < threshold` (the "reject" branch)
 *   - Output slot 1 ("pass"):  candidates with `score >= threshold` (the "accept" branch)
 *
 * Typical use is to put a `FilterPipe` after a `ResultsSummaryPipe` to peel survivors
 * (route into a checkpoint sink / promotion stage) from the rest of the population (route
 * back into a multiplexer for another round of mutation, or to a NullSink to discard).
 *
 * Saturation semantics: ready as soon as any input is queued (like the other passthroughs,
 * to avoid stalling on a slow upstream). The base-class `outputsAreSaturated()` is fine
 * here -- it returns true only when *both* output slots are full, which lets the pipe keep
 * working as long as at least one branch has space. Per-item routing is back-pressured: if
 * the next candidate's target slot is full, the pipe stops processing until that slot
 * drains, deliberately *not* re-routing into the other slot (re-routing would silently
 * misclassify survivors, which is the bug this pipe exists to prevent).
 */
class FilterPipe : public Pipe {
 public:
  /**
   * @param max_candidates Per-slot buffer capacity.
   * @param threshold      Fitness boundary. Strictly less goes to slot 0; equal-or-above
   *                       to slot 1. Choose a value in the same numeric range your
   *                       upstream evaluator produces (typically [0, 1]).
   */
  FilterPipe(uint32_t max_candidates, double threshold);

  /**
   * @brief Peek the next candidate, route it to slot 0 or 1 based on the threshold
   */
  void execute() override;

  /**
   * @brief Ready as soon as any input is queued.
   *
   * Same rationale as ResultsSummaryPipe / DemultiplexerPipe -- the base "all slots full"
   * gate would silently stall this pipe behind a slow upstream.
   */
  [[nodiscard]] bool inputsAreSaturated() override;

  [[nodiscard]] double getThreshold() const noexcept;

 private:
  double threshold_;
};

} // namespace beast

#endif // BEAST_PIPES_FILTER_PIPE_HPP_
