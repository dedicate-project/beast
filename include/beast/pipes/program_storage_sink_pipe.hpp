#ifndef BEAST_PIPES_PROGRAM_STORAGE_SINK_PIPE_HPP_
#define BEAST_PIPES_PROGRAM_STORAGE_SINK_PIPE_HPP_

// Standard
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class ProgramStorageSinkPipe
 * @brief Persists the highest-scoring candidates that flow through it to disk
 *
 * Sit one of these downstream of an evaluator (or a `ResultsSummaryPipe`) to keep an
 * on-disk record of the best programs the pipeline has produced. Pairs naturally with
 * `ProgramStorageSourcePipe`, which reads back from the same file -- together they form
 * a simple checkpoint loop: stop the pipeline, bump the maze difficulty, restart it
 * with a `ProgramStorageSourcePipe` seeding the factory's slot with last run's
 * survivors.
 *
 * File format
 * - A single JSON file whose contents are a list of `{score, data}` objects, sorted by
 *   descending score, capped at `top_k` entries.
 * - We rewrite the file atomically (write-temp + rename) on every batch flush so the
 *   reader side never sees a half-written file.
 *
 * Construction
 * - `max_candidates`: per-input-slot buffer capacity.
 * - `path`: absolute or relative file path where the JSON ledger lives. Empty path
 *   makes the pipe a degraded "log nothing" passthrough -- which is useful as a default
 *   when the user adds the pipe via the UI before configuring its destination.
 * - `top_k`: maximum number of entries to retain. 0 falls back to a sensible default
 *   (10).
 */
class ProgramStorageSinkPipe : public Pipe {
 public:
  /**
   * @param max_candidates Per-slot buffer capacity
   * @param path           Destination JSON ledger; empty disables persistence
   * @param top_k          Maximum retained entries (0 picks the default of 10)
   */
  ProgramStorageSinkPipe(uint32_t max_candidates, std::string path, uint32_t top_k = 0);

  /**
   * @brief Drain the input slot, fold new entries into the top-K, persist on disk
   *
   * Only writes the file when the top-K actually changed -- a sustained stream of
   * low-scoring candidates that never beats the current record won't churn the disk.
   */
  void execute() override;

  /**
   * @brief On-disk destination of the JSON ledger (empty when disabled)
   */
  [[nodiscard]] const std::string& getPath() const noexcept;

  /**
   * @brief Maximum number of entries retained
   */
  [[nodiscard]] uint32_t getTopK() const noexcept;

  /**
   * @brief Snapshot of the currently retained entries
   */
  [[nodiscard]] std::vector<OutputItem> getEntries() const;

 private:
  bool foldCandidate(const OutputItem& candidate);
  void persistLocked() const;

  std::string path_;
  uint32_t top_k_;

  mutable std::mutex entries_mutex_;
  std::vector<OutputItem> entries_; // sorted by descending score
};

} // namespace beast

#endif // BEAST_PIPES_PROGRAM_STORAGE_SINK_PIPE_HPP_
