#ifndef BEAST_PIPES_MULTIPLEXER_PIPE_HPP_
#define BEAST_PIPES_MULTIPLEXER_PIPE_HPP_

// Standard
#include <cstdint>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class MultiplexerPipe
 * @brief Merges candidates from N input slots into a single output slot
 *
 * Combine several upstream sources into one pipe with this. Each tick the pipe walks
 * round-robin across its input slots, taking whichever has candidates pending, and forwards
 * them (score-preserving) to the single output slot. Round-robin is intentional rather
 * than "drain slot 0 first" so a busy upstream can't starve a slower upstream.
 *
 * Use cases
 * - Loop-back: a downstream pipe's output is fed into one of this pipe's inputs alongside
 *   a fresh ProgramFactoryPipe, so survivors get a chance to re-evolve.
 * - Ensembles: merge candidates from several factories that emit different starting
 *   distributions and let downstream evolution pick the best.
 *
 * Construction
 * - `max_candidates` controls both per-input-slot capacity and output capacity.
 * - `input_slots` is configurable; 0 or 1 collapse to a single passthrough input.
 */
class MultiplexerPipe : public Pipe {
 public:
  /**
   * @param max_candidates Per-slot buffer capacity
   * @param input_slots    Number of input slots to expose (capped at 16 for sanity)
   */
  MultiplexerPipe(uint32_t max_candidates, uint32_t input_slots);

  /**
   * @brief Round-robin drain of every input slot into the single output slot
   *
   * Stops when either every input slot is empty or the output slot saturates.
   */
  void execute() override;

  /**
   * @brief Ready as soon as any input slot has something to drain.
   *
   * The base-class default requires *every* slot to be full, which would deadlock a
   * mux whose only purpose is to feed from a sparsely-active loop-back: if input 1
   * (the survivor loop) is slow to produce, input 0 (a fresh factory) would never
   * see execute() called and items would pile up at the input until max_candidates.
   */
  [[nodiscard]] bool inputsAreSaturated() override;

  /**
   * @brief Number of input slots exposed by this pipe
   */
  [[nodiscard]] uint32_t getInputSlots() const noexcept;

 private:
  uint32_t input_slot_count_;
  uint32_t round_robin_cursor_ = 0;
};

} // namespace beast

#endif // BEAST_PIPES_MULTIPLEXER_PIPE_HPP_
