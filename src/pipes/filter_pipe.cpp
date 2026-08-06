#include <beast/pipes/filter_pipe.hpp>

// Standard
#include <utility>

namespace beast {

namespace {
constexpr uint32_t kRejectSlot = 0;
constexpr uint32_t kPassSlot = 1;
} // namespace

FilterPipe::FilterPipe(uint32_t max_candidates, double threshold)
    : Pipe(max_candidates, /*input_slots=*/1, /*output_slots=*/2), threshold_{threshold} {}

bool FilterPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

void FilterPipe::execute() {
  // We have to peek *before* drawing so we know which slot the next item should land in.
  // Drawing first and then discovering the target is full would force us to push the item
  // back onto the input -- almost certainly behind newer arrivals, which silently reorders
  // candidates and breaks the FIFO invariant the rest of the pipeline relies on.
  while (getInputSlotAmount(0) > 0) {
    const double score = peekInputScore(0);
    const uint32_t target = (score < threshold_) ? kRejectSlot : kPassSlot;
    if (getOutputSlotAmount(target) >= getMaxCandidates()) {
      // Target branch is full; stop and apply back-pressure. We deliberately don't reroute
      // to the other slot -- the whole point of a filter is that survivors and rejects
      // stay on their own rails, even when one rail is congested.
      return;
    }
    auto item = drawInputWithScore(0);
    storeOutput(target, std::move(item));
  }
}

double FilterPipe::getThreshold() const noexcept { return threshold_; }

} // namespace beast
