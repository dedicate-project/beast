#ifndef BEAST_PIPES_RESULTS_SUMMARY_PIPE_HPP_
#define BEAST_PIPES_RESULTS_SUMMARY_PIPE_HPP_

// Standard
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class ResultsSummaryPipe
 * @brief Passthrough pipe that surfaces score statistics of the candidates flowing through it
 *
 * Sit one of these between any two pipes (e.g. between an EvaluatorPipe and a NullSinkPipe)
 * to get a live read-out of how candidates are scoring without having to interrupt the
 * pipeline. Every candidate is forwarded unchanged (score included), so adding/removing this
 * pipe doesn't alter what downstream pipes see.
 *
 * The pipe keeps a rolling window of the most recent N candidates' scores so the per-pipe
 * `Summary` metric the HTTP layer exposes reflects the *current* training behaviour rather
 * than the all-time average. It also remembers the single best score and the genome that
 * produced it for the entire lifetime of the pipe -- useful for "show me the best program
 * the maze evaluator ever produced".
 *
 * Construction parameters
 * - `max_candidates`: input/output slot capacity (matches the other pipes' semantics).
 * - `window_size`:    how many of the most-recent scores are kept in the rolling stats.
 *                     0 falls back to a sensible default (256).
 *
 * Thread-safety: all `get*` accessors are safe to call concurrently with `execute()` (they
 * take the same internal mutex). They return immutable snapshots so callers don't have to
 * hold the lock themselves.
 */
class ResultsSummaryPipe : public Pipe {
 public:
  /**
   * @brief Rolling score statistics for the most recent `window_size` candidates
   *
   * `count_total` keeps growing for the life of the pipe; `count_window` plateaus at
   * `window_size`. `best_ever_score` / `best_ever_data` reflect the all-time best.
   */
  struct Summary {
    uint64_t count_total = 0;
    uint64_t count_window = 0;
    double min_score = 0.0;
    double max_score = 0.0;
    double mean_score = 0.0;
    double last_score = 0.0;
    double best_ever_score = 0.0;
    std::vector<unsigned char> best_ever_data;
  };

  /**
   * @param max_candidates Input/output slot capacity for the pipe's internal buffers
   * @param window_size    Maximum number of recent scores to keep for rolling statistics;
   *                       0 selects the implementation default (256)
   */
  explicit ResultsSummaryPipe(uint32_t max_candidates, uint32_t window_size = 0);

  /**
   * @brief Drain inputs, accumulate stats, forward to output preserving score
   *
   * Forwarding stops as soon as the output slot has no more space; remaining inputs are
   * left in the input queue for the next tick (same back-pressure shape as other pipes).
   */
  void execute() override;

  /**
   * @brief Snapshot of the current rolling statistics
   */
  [[nodiscard]] Summary getSummary() const;

  /**
   * @brief Reset the rolling and all-time statistics
   *
   * Useful after the user bumps the maze difficulty so the summary reflects the new
   * regime instead of mixing in scores from the easier maze.
   */
  void resetSummary();

  /**
   * @brief Rolling window capacity in number of recent candidates
   */
  [[nodiscard]] uint32_t getWindowSize() const noexcept;

  /**
   * @brief Opaque snapshot of the pipe's running statistics
   *
   * Used by `PipelineManager::mutatePipeline` to carry the stats across a structural
   * edit (which otherwise rebuilds the pipe from JSON and would zero them out -- the
   * "best ever resets to 0 when I edit anything" failure mode).
   */
  struct PersistentState {
    std::deque<double> recent_scores;
    uint64_t count_total = 0;
    double last_score = 0.0;
    double best_ever_score = 0.0;
    std::vector<unsigned char> best_ever_data;
    bool has_any = false;
  };

  /**
   * @brief Snapshot the current statistics so a caller can re-apply them later
   */
  [[nodiscard]] PersistentState exportState() const;

  /**
   * @brief Restore statistics previously captured via `exportState`
   *
   * The rolling window is truncated to the current `window_size_` if the imported
   * state was captured with a larger window.
   */
  void importState(PersistentState state);

 private:
  void recordScore(double score, const std::vector<unsigned char>& data);

  uint32_t window_size_;

  mutable std::mutex summary_mutex_;
  std::deque<double> recent_scores_;
  uint64_t count_total_ = 0;
  double last_score_ = 0.0;
  double best_ever_score_ = 0.0;
  std::vector<unsigned char> best_ever_data_;
  bool has_any_ = false;
};

} // namespace beast

#endif // BEAST_PIPES_RESULTS_SUMMARY_PIPE_HPP_
