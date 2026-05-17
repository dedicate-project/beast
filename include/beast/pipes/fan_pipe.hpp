#ifndef BEAST_PIPES_FAN_PIPE_HPP_
#define BEAST_PIPES_FAN_PIPE_HPP_

// Standard
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class FanPipe
 * @brief Passthrough that reports how fast candidates are flowing through it
 *
 * The pipe forwards every candidate unchanged (score-preserving) and tracks throughput
 * over a sliding window. The UI renders a fan whose spin rate is proportional to the
 * reported `candidates_per_second`, giving an immediate visual cue for which pipes are
 * doing work and which are bottlenecked. The throughput is also exposed via the metrics
 * endpoint so it can drive automation (e.g. autoscaling, alerts).
 *
 * Construction
 * - `max_candidates`: per-slot buffer capacity (matches the rest of the lineup).
 * - `window_seconds`: width of the sliding window used to compute the throughput; the
 *   shorter the window the snappier the reading. 0 falls back to a sensible default
 *   (2 seconds).
 */
class FanPipe : public Pipe {
 public:
  struct Throughput {
    uint64_t total_seen = 0;
    uint64_t window_seen = 0;
    double window_seconds = 0.0;
    double candidates_per_second = 0.0;
  };

  /**
   * @param max_candidates Per-slot buffer capacity
   * @param window_seconds Sliding window width in seconds (0 = default of 2 s)
   */
  FanPipe(uint32_t max_candidates, double window_seconds = 0.0);

  /**
   * @brief Drain input, count each item, and forward (score-preserving) to the output
   */
  void execute() override;

  /**
   * @brief Snapshot of the current throughput reading
   */
  [[nodiscard]] Throughput getThroughput() const;

  /**
   * @brief Width of the sliding window in seconds
   */
  [[nodiscard]] double getWindowSeconds() const noexcept;

 private:
  using Clock = std::chrono::steady_clock;

  double window_seconds_;

  mutable std::mutex stats_mutex_;
  uint64_t total_seen_ = 0;
  // Each entry is (timestamp, count_at_that_tick). We trim entries older than
  // window_seconds_ inside getThroughput (hence `mutable`). We don't need per-candidate
  // timestamps; bucketing by tick is fine and keeps the deque short even under heavy
  // throughput.
  struct Sample {
    Clock::time_point ts;
    uint64_t count;
  };
  mutable std::deque<Sample> samples_;
};

} // namespace beast

#endif // BEAST_PIPES_FAN_PIPE_HPP_
