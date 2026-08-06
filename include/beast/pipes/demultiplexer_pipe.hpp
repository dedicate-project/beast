#ifndef BEAST_PIPES_DEMULTIPLEXER_PIPE_HPP_
#define BEAST_PIPES_DEMULTIPLEXER_PIPE_HPP_

// Standard
#include <cstdint>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class DemultiplexerPipe
 * @brief Splits candidates from a single input slot across N output slots
 *
 * Mirror of `MultiplexerPipe`. Configurable strategy:
 * - `Strategy::RoundRobin` (default): each candidate goes to the next output slot in
 *   sequence, giving every downstream branch an even fan-out. Strict back-pressure: if
 *   the next slot in the cycle is full, the pipe stalls instead of skipping ahead. Use
 *   this when downstream branches have *symmetric* throughput and you want the slow
 *   branch to apply back-pressure to the entire upstream.
 * - `Strategy::Broadcast`: each candidate is copied to *every* output slot. Useful when
 *   each branch evaluates the same population against a different task.
 * - `Strategy::LeastLoaded`: each candidate goes to the output slot that currently has
 *   the *fewest* queued items (round-robin tie-break across equally-loaded slots). This
 *   is the right choice when downstream branches have *asymmetric* throughput -- a slow
 *   branch's full buffer no longer starves the fast branches because the demux just
 *   routes around it. The trade-off vs. RoundRobin is that the slow branch sees a much
 *   smaller share of candidates instead of an even split, which is usually what you
 *   want (otherwise the fast branch idles waiting for the slow branch to drain). Falls
 *   back to the round-robin behaviour when *all* slots are equally full -- so behaves
 *   identically to RoundRobin in the symmetric case.
 *
 * Construction
 * - `max_candidates` controls per-slot buffer capacity.
 * - `output_slots` is configurable (capped at 16).
 */
class DemultiplexerPipe : public Pipe {
 public:
  enum class Strategy {
    RoundRobin,
    Broadcast,
    LeastLoaded,
  };

  DemultiplexerPipe(uint32_t max_candidates, uint32_t output_slots,
                    Strategy strategy = Strategy::RoundRobin);

  /**
   * @brief Drain input, route each candidate per the configured strategy
   */
  void execute() override;

  /**
   * @brief Ready as soon as the single input slot has anything to fan out.
   *
   * See ResultsSummaryPipe::inputsAreSaturated for the rationale.
   */
  [[nodiscard]] bool inputsAreSaturated() override;

  [[nodiscard]] uint32_t getOutputSlots() const noexcept;
  [[nodiscard]] Strategy getStrategy() const noexcept;

 private:
  uint32_t output_slot_count_;
  Strategy strategy_;
  uint32_t round_robin_cursor_ = 0;
};

} // namespace beast

#endif // BEAST_PIPES_DEMULTIPLEXER_PIPE_HPP_
