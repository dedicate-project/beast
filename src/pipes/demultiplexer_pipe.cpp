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

bool DemultiplexerPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

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
    } else if (strategy_ == Strategy::LeastLoaded) {
      // Pick the output slot with the fewest queued items, breaking ties with the
      // round-robin cursor so equally-loaded slots still fan out evenly. This is the
      // right routing policy when downstream branches have asymmetric throughput: the
      // slow branch's buffer fills, and the demux quietly stops sending to it instead
      // of stalling the whole pipeline (which is what RoundRobin would do). When every
      // slot is at capacity we bail with back-pressure -- there's no "least-bad" target
      // to pick once they're all full.
      uint32_t best_slot = round_robin_cursor_;
      uint32_t best_load = getOutputSlotAmount(best_slot);
      for (uint32_t step = 1; step < output_slot_count_; ++step) {
        const uint32_t slot = (round_robin_cursor_ + step) % output_slot_count_;
        const uint32_t load = getOutputSlotAmount(slot);
        if (load < best_load) {
          best_load = load;
          best_slot = slot;
        }
      }
      if (best_load >= getMaxCandidates()) {
        return;
      }
      auto item = drawInputWithScore(0);
      storeOutput(best_slot, std::move(item));
      // Advance the cursor past the just-served slot so the next tie-break breaks the
      // other way. Without this the cursor would sit on the lightest slot indefinitely
      // and a burst of identically-empty slots would always land on the lowest index.
      round_robin_cursor_ = (best_slot + 1) % output_slot_count_;
    } else {
      // Round-robin: send to the next slot and advance the cursor. Stop the tick if the
      // chosen slot is full -- we deliberately don't skip ahead to a non-full slot so
      // we apply correct back-pressure (the user's wiring usually implies "go here next"
      // and quietly rerouting can mask a downstream bottleneck). If you want the demux
      // to *skip* a saturated slot rather than stall, use `Strategy::LeastLoaded`.
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
