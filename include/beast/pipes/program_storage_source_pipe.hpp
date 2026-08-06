#ifndef BEAST_PIPES_PROGRAM_STORAGE_SOURCE_PIPE_HPP_
#define BEAST_PIPES_PROGRAM_STORAGE_SOURCE_PIPE_HPP_

// Standard
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class ProgramStorageSourcePipe
 * @brief Loads previously persisted candidate programs from disk and emits them
 *
 * Companion to `ProgramStorageSinkPipe`. Reads the same JSON ledger format and re-emits
 * the entries through its single output slot. Use as a seeding source -- feed it into a
 * `MultiplexerPipe` alongside a fresh `ProgramFactoryPipe` to start a new run with a
 * mix of last run's survivors and fresh exploration.
 *
 * On-disk refresh
 * - The file is re-read in the background whenever its mtime advances (rate-limited to
 *   roughly once per second to keep the syscall cost bounded). This is the key to the
 *   staged-pipeline pattern: a downstream `ProgramStorageSinkPipe` writing the same
 *   file from another stage will be picked up automatically without restarting the
 *   pipeline. If the file is missing or empty at construction, the source simply waits
 *   for it to appear -- there's no "load once at startup" race.
 * - When a refresh adds or removes entries, the read cursor resets to the top of the
 *   new snapshot so freshly-promoted survivors are emitted first.
 *
 * Construction
 * - `max_candidates`: per-slot buffer capacity.
 * - `path`: source JSON ledger file. Empty path makes the pipe a no-op source.
 * - `loop`: when true, the source restarts from the beginning after exhausting the
 *   ledger; useful for ongoing seeding. When false, the source emits each entry once
 *   and then sits idle (but still re-reads the file when it changes -- a fresh write
 *   un-sets the exhausted flag).
 */
class ProgramStorageSourcePipe : public Pipe {
 public:
  /**
   * @param max_candidates Per-slot buffer capacity
   * @param path           Source JSON ledger; empty disables emission
   * @param loop           Restart from the beginning after exhausting entries
   */
  ProgramStorageSourcePipe(uint32_t max_candidates, std::string path, bool loop);

  /**
   * @brief Push as many cached entries as fit into the output buffer this tick
   */
  void execute() override;

  [[nodiscard]] const std::string& getPath() const noexcept;
  [[nodiscard]] bool getLoop() const noexcept;

  /**
   * @brief Number of entries currently cached from the on-disk ledger
   *
   * Refreshed on each `execute()` call; not necessarily the same as the count at
   * construction time. May be 0 if the file is empty, missing, or unreadable.
   */
  [[nodiscard]] uint32_t getEntryCount() const noexcept;

 private:
  /// Drop the cached entries on the floor and reload from `path_`. Returns silently on
  /// any I/O or parse failure (the previously cached entries stay intact in that case
  /// -- best-effort persistence, not a hard dependency). Called under `entries_mutex_`.
  void reloadFromDiskLocked();

  /// Stat the file; if the mtime has advanced since the last load, reload. Rate-limited
  /// so we don't stat() on every execute() tick when the source is in a hot empty
  /// spin (no entries cached, output never saturates, worker loops at MHz). Called
  /// under `entries_mutex_`.
  void maybeRefreshFromDiskLocked();

  std::string path_;
  bool loop_;

  /// Guards entries_, cursor_, exhausted_, last_check_, last_loaded_mtime_. All
  /// non-const access to those fields happens via `execute()` (single-threaded by the
  /// pipeline worker) and `getEntryCount()` (called from the metrics scraper), so the
  /// mutex is mostly for the metrics scraper's benefit.
  mutable std::mutex entries_mutex_;
  std::vector<OutputItem> entries_;
  uint32_t cursor_ = 0;
  bool exhausted_ = false;

  /// Wall-clock time of the most recent stat() call. Used by
  /// `maybeRefreshFromDiskLocked` to throttle re-reads.
  std::chrono::steady_clock::time_point last_check_ = {};

  /// File mtime at the most recent successful load. Default-constructed to a
  /// sentinel that won't match any real mtime, so the first refresh always reloads.
  std::filesystem::file_time_type last_loaded_mtime_ = {};
};

} // namespace beast

#endif // BEAST_PIPES_PROGRAM_STORAGE_SOURCE_PIPE_HPP_
