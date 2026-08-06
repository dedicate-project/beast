#ifndef BEAST_PIPES_SCORE_GRAPH_PIPE_HPP_
#define BEAST_PIPES_SCORE_GRAPH_PIPE_HPP_

// Standard
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class ScoreGraphPipe
 * @brief Passthrough pipe that records a time-series of candidate scores
 *
 * Sits between any two pipes (the same place you'd put a `ResultsSummaryPipe`) and
 * silently records every candidate's score along with the wall-clock time it was seen.
 * The UI renders the resulting series as a sparkline on hover and a full-size line
 * graph in the right-hand summaries panel; the pipe itself just collects samples and
 * answers `getSnapshot()` calls so the HTTP metrics endpoint can serialise them.
 *
 * Window semantics
 * - `window_seconds`: samples older than `now - window_seconds` are pruned on every
 *   `getSnapshot()` and every `recordScore()` call. The "current window" the user sees
 *   in the graph is always this many seconds wide. Default 60 s.
 * - `max_samples`:    hard cap on the deque length so a screamingly fast pipeline at
 *                     a few kHz can't pin the snapshot CPU budget on a multi-minute
 *                     window. When the cap is hit, oldest samples are dropped first.
 *                     Default 1024.
 *
 * Reset behaviour
 * - Reset on pipeline restart: the pipe's `start()` / construction zeroes the series.
 *   "It's ok to have it reset the graph upon restarting the pipeline" -- and indeed
 *   the natural behaviour of constructing a fresh pipe on pipeline-start gives us
 *   that for free without explicit teardown.
 *
 * Thread-safety: all accessors take an internal mutex, same idiom as
 * `ResultsSummaryPipe`. Snapshots return immutable copies so callers don't have to
 * hold the lock themselves.
 */
class ScoreGraphPipe : public Pipe {
 public:
  /// One sample in the rolling series. Time is the offset in seconds from the pipe's
  /// construction; a monotonic steady_clock baseline keeps the series immune to wall
  /// clock adjustments that would otherwise warp the x-axis mid-run.
  struct Sample {
    double t_seconds;
    double score;
  };

  /// Snapshot of the current rolling time-series + summary stats. The samples vector
  /// is ordered oldest-first so the UI can stream it straight into an SVG polyline
  /// without re-sorting; everything is a value copy so the caller can outlive the
  /// pipe without holding the lock.
  struct Snapshot {
    std::vector<Sample> samples;
    double window_seconds = 0.0;
    uint64_t total_seen = 0;
    double min_score = 0.0;
    double max_score = 0.0;
    double mean_score = 0.0;
    double last_score = 0.0;
  };

  /**
   * @param max_candidates Input/output slot capacity for the pipe's internal buffers.
   * @param window_seconds How many seconds of history the graph keeps. 0 selects the
   *                       implementation default (60).
   * @param max_samples    Hard cap on the number of samples kept in the rolling
   *                       deque. 0 selects the implementation default (1024).
   */
  explicit ScoreGraphPipe(uint32_t max_candidates, double window_seconds = 0.0,
                          uint32_t max_samples = 0);

  /// Drain inputs, record one sample per candidate, forward to output preserving
  /// score. Same back-pressure shape as ResultsSummaryPipe.
  void execute() override;

  /// As soon as any candidate has arrived. Same observer-pipe pattern as the rest of
  /// the passthrough family (NullSink / ResultsSummary / Filter) -- the default
  /// "every slot must be full" gate is wrong for one-slot pipes that should fire on
  /// any input.
  [[nodiscard]] bool inputsAreSaturated() override;

  /// Snapshot of the current time-series + summary stats. Prunes expired samples
  /// before returning so the caller never sees out-of-window history.
  [[nodiscard]] Snapshot getSnapshot() const;

  [[nodiscard]] double getWindowSeconds() const noexcept;
  [[nodiscard]] uint32_t getMaxSamples() const noexcept;

 private:
  void recordScore(double score);
  [[nodiscard]] double secondsSinceStart() const noexcept;
  void prune(double now_seconds);

  const double window_seconds_;
  const uint32_t max_samples_;
  const std::chrono::steady_clock::time_point started_at_;

  mutable std::mutex graph_mutex_;
  std::deque<Sample> samples_;
  uint64_t total_seen_ = 0;
  double last_score_ = 0.0;
};

} // namespace beast

#endif // BEAST_PIPES_SCORE_GRAPH_PIPE_HPP_
