#include <beast/pipes/fan_pipe.hpp>

// Standard
#include <algorithm>
#include <utility>

namespace beast {

namespace {
constexpr double kDefaultWindowSeconds = 2.0;
} // namespace

FanPipe::FanPipe(uint32_t max_candidates, double window_seconds)
    : Pipe(max_candidates, /*input_slots=*/1, /*output_slots=*/1),
      window_seconds_(window_seconds <= 0.0 ? kDefaultWindowSeconds : window_seconds) {}

bool FanPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

void FanPipe::execute() {
  uint64_t handled_this_tick = 0;
  while (getInputSlotAmount(0) > 0) {
    if (outputsAreSaturated()) {
      break;
    }
    auto item = drawInputWithScore(0);
    storeOutput(0, std::move(item));
    ++handled_this_tick;
  }
  if (handled_this_tick == 0) {
    return;
  }
  std::scoped_lock lock(stats_mutex_);
  total_seen_ += handled_this_tick;
  samples_.push_back({Clock::now(), handled_this_tick});
}

FanPipe::Throughput FanPipe::getThroughput() const {
  std::scoped_lock lock(stats_mutex_);
  const auto now = Clock::now();
  const auto window =
      std::chrono::duration<double>(window_seconds_);
  const auto cutoff = now - std::chrono::duration_cast<Clock::duration>(window);

  // Drop entries that have aged out of the window. samples_ is `mutable` precisely so
  // this housekeeping can live inside the const accessor; the visible state ("how many
  // candidates went through in the last N seconds") is unchanged by the prune.
  while (!samples_.empty() && samples_.front().ts < cutoff) {
    samples_.pop_front();
  }

  Throughput out;
  out.total_seen = total_seen_;
  out.window_seconds = window_seconds_;
  for (const auto& sample : samples_) {
    out.window_seen += sample.count;
  }
  out.candidates_per_second = out.window_seconds > 0.0
                                  ? static_cast<double>(out.window_seen) / out.window_seconds
                                  : 0.0;
  return out;
}

double FanPipe::getWindowSeconds() const noexcept { return window_seconds_; }

} // namespace beast
