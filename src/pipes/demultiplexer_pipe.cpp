#include <beast/pipes/demultiplexer_pipe.hpp>

// Standard
#include <algorithm>
#include <utility>

namespace beast {

namespace {
constexpr uint32_t kMaxOutputSlots = 16;

uint32_t clampSlots(uint32_t requested) {
  if (requested == 0) {
    return 1;
  }
  return std::min(requested, kMaxOutputSlots);
}
} // namespace

DemultiplexerPipe::DemultiplexerPipe(uint32_t max_candidates, uint32_t output_slots,
                                     Strategy strategy)
    : Pipe(max_candidates, 1, clampSlots(output_slots)),
      output_slot_count_(clampSlots(output_slots)),
      strategy_(strategy) {}

void DemultiplexerPipe::execute() {
  // Always check downstream capacity BEFORE drawing from the input. If we drew first and
  // then discovered we couldn't route, we'd have to push the item back onto the input
  // queue's tail, which silently reorders behind any candidates that arrived in the
  // meantime -- subtle FIFO violation that can starve early candidates indefinitely.
  while (getInputSlotAmount(0) > 0) {
    if (strategy_ == Strategy::Broadcast) {
      // Broadcast needs every output slot to have at least one free spot.
      bool blocked = false;
      for (uint32_t slot = 0; slot < output_slot_count_; ++slot) {
        if (getOutputSlotAmount(slot) >= getMaxCandidates()) {
          blocked = true;
          break;
        }
      }
      if (blocked) {
        return;
      }
      auto item = drawInputWithScore(0);
      // Copy to every slot except the last, which gets a move so we don't pay for a
      // redundant clone. Doing this in two passes keeps the moved-from access pattern
      // obviously linear, which keeps clang-tidy happy too.
      for (uint32_t slot = 0; slot + 1 < output_slot_count_; ++slot) {
        storeOutput(slot, item);
      }
      storeOutput(output_slot_count_ - 1, std::move(item));
    } else {
      // Round-robin: send to the next slot and advance the cursor. Stop the tick if the
      // chosen slot is full -- we deliberately don't skip ahead to a non-full slot so
      // we apply correct back-pressure (the user's wiring usually implies "go here next"
      // and quietly rerouting can mask a downstream bottleneck).
      const uint32_t target = round_robin_cursor_;
      if (getOutputSlotAmount(target) >= getMaxCandidates()) {
        return;
      }
      auto item = drawInputWithScore(0);
      storeOutput(target, std::move(item));
      round_robin_cursor_ = (round_robin_cursor_ + 1) % output_slot_count_;
    }
  }
}

uint32_t DemultiplexerPipe::getOutputSlots() const noexcept { return output_slot_count_; }

DemultiplexerPipe::Strategy DemultiplexerPipe::getStrategy() const noexcept { return strategy_; }

} // namespace beast
