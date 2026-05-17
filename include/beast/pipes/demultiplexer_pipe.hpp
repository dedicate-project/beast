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
 *   sequence, giving every downstream branch an even fan-out. Useful for splitting work
 *   across parallel evolution stages.
 * - `Strategy::Broadcast`: each candidate is copied to *every* output slot. Useful when
 *   each branch evaluates the same population against a different task.
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
