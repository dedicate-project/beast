#include <beast/pipes/multiplexer_pipe.hpp>

// Standard
#include <algorithm>
#include <utility>

namespace beast {

namespace {
constexpr uint32_t kMaxInputSlots = 16;

uint32_t clampSlots(uint32_t requested) {
  if (requested == 0) {
    return 1;
  }
  return std::min(requested, kMaxInputSlots);
}
} // namespace

MultiplexerPipe::MultiplexerPipe(uint32_t max_candidates, uint32_t input_slots)
    : Pipe(max_candidates, clampSlots(input_slots), 1),
      input_slot_count_(clampSlots(input_slots)) {}

bool MultiplexerPipe::inputsAreSaturated() {
  for (uint32_t slot = 0; slot < input_slot_count_; ++slot) {
    if (getInputSlotAmount(slot) > 0) {
      return true;
    }
  }
  return false;
}

void MultiplexerPipe::execute() {
  // Round-robin walk: visit every input slot starting from the cursor, take one item per
  // visit, advance. We do up to input_slot_count_ rounds before re-checking whether
  // anything's still available so a stuck slot can't burn the loop infinitely.
  while (true) {
    bool emitted_any = false;
    for (uint32_t step = 0; step < input_slot_count_; ++step) {
      const uint32_t slot = (round_robin_cursor_ + step) % input_slot_count_;
      if (getInputSlotAmount(slot) == 0) {
        continue;
      }
      auto item = drawInputWithScore(slot);
      storeOutput(0, std::move(item));
      emitted_any = true;
      if (outputsAreSaturated()) {
        // Resume from the next slot on the next tick so a hot slot 0 doesn't drown the
        // others when downstream is the bottleneck.
        round_robin_cursor_ = (slot + 1) % input_slot_count_;
        return;
      }
    }
    if (!emitted_any) {
      return;
    }
    round_robin_cursor_ = (round_robin_cursor_ + 1) % input_slot_count_;
  }
}

uint32_t MultiplexerPipe::getInputSlots() const noexcept { return input_slot_count_; }

} // namespace beast
