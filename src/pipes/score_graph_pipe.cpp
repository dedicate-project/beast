#include <beast/pipes/score_graph_pipe.hpp>

// Standard
#include <algorithm>
#include <numeric>
#include <utility>

namespace beast {

namespace {
constexpr double kDefaultWindowSeconds = 60.0;
constexpr uint32_t kDefaultMaxSamples = 1024;
} // namespace

ScoreGraphPipe::ScoreGraphPipe(uint32_t max_candidates, double window_seconds,
                               uint32_t max_samples)
    : Pipe(max_candidates, 1, 1),
      // Treat 0.0 and any negative value as "use the default" -- the only sensible
      // alternative would be "track everything forever" which would let the deque
      // grow unbounded on a long run.
      window_seconds_(window_seconds > 0.0 ? window_seconds : kDefaultWindowSeconds),
      max_samples_(max_samples == 0 ? kDefaultMaxSamples : max_samples),
      started_at_(std::chrono::steady_clock::now()) {}

bool ScoreGraphPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

void ScoreGraphPipe::execute() {
  // Mirror ResultsSummaryPipe: drain everything in the input slot each tick, but stop
  // forwarding once the output slot fills so upstream picks up the back-pressure
  // signal naturally.
  while (getInputSlotAmount(0) > 0) {
    auto item = drawInputWithScore(0);
    recordScore(item.score);
    storeOutput(0, std::move(item));
    if (outputsAreSaturated()) {
      break;
    }
  }
}

double ScoreGraphPipe::secondsSinceStart() const noexcept {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(now - started_at_).count();
}

void ScoreGraphPipe::recordScore(double score) {
  std::scoped_lock lock(graph_mutex_);
  const double now = secondsSinceStart();
  samples_.push_back({now, score});
  last_score_ = score;
  ++total_seen_;
  // Two pruning passes: (1) time-window, (2) size cap. The order matters -- time pruning
  // is the more useful one (matches the user-facing "graph shows the last N seconds"
  // mental model), and the size cap is just the memory-safety floor that kicks in when
  // the pipe is hammered faster than `max_samples_ / window_seconds_` per second.
  prune(now);
  while (samples_.size() > max_samples_) {
    samples_.pop_front();
  }
}

void ScoreGraphPipe::prune(double now_seconds) {
  const double cutoff = now_seconds - window_seconds_;
  while (!samples_.empty() && samples_.front().t_seconds < cutoff) {
    samples_.pop_front();
  }
}

ScoreGraphPipe::Snapshot ScoreGraphPipe::getSnapshot() const {
  std::scoped_lock lock(graph_mutex_);
  // const-correctness wart: we prune from a non-const helper so the snapshot reflects
  // the current window. Cast the mutex-protected mutable state to non-const through a
  // local copy of the deque after pruning into a temp -- this keeps `getSnapshot()`
  // const in the API while letting the snapshot drop expired entries. Cheapest
  // approach: do the pruning into a fresh deque, leave the member state alone here.
  const double now = secondsSinceStart();
  const double cutoff = now - window_seconds_;
  Snapshot out;
  out.window_seconds = window_seconds_;
  out.total_seen = total_seen_;
  out.last_score = last_score_;
  for (const auto& sample : samples_) {
    if (sample.t_seconds < cutoff) {
      continue;
    }
    out.samples.push_back(sample);
  }
  if (!out.samples.empty()) {
    auto extract = [](const Sample& s) { return s.score; };
    out.min_score = extract(*std::min_element(
        out.samples.begin(), out.samples.end(),
        [](const Sample& a, const Sample& b) { return a.score < b.score; }));
    out.max_score = extract(*std::max_element(
        out.samples.begin(), out.samples.end(),
        [](const Sample& a, const Sample& b) { return a.score < b.score; }));
    out.mean_score = std::accumulate(out.samples.begin(), out.samples.end(), 0.0,
                                     [](double acc, const Sample& s) {
                                       return acc + s.score;
                                     }) /
                     static_cast<double>(out.samples.size());
  }
  return out;
}

double ScoreGraphPipe::getWindowSeconds() const noexcept { return window_seconds_; }

uint32_t ScoreGraphPipe::getMaxSamples() const noexcept { return max_samples_; }

} // namespace beast
