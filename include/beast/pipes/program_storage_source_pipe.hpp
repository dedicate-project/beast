#ifndef BEAST_PIPES_PROGRAM_STORAGE_SOURCE_PIPE_HPP_
#define BEAST_PIPES_PROGRAM_STORAGE_SOURCE_PIPE_HPP_

// Standard
#include <cstdint>
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
 * Construction
 * - `max_candidates`: per-slot buffer capacity.
 * - `path`: source JSON ledger file. Empty path makes the pipe a no-op source.
 * - `loop`: when true, the source restarts from the beginning after exhausting the
 *   ledger; useful for ongoing seeding. When false, the source emits each entry once
 *   and then sits idle.
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
   * @brief Number of entries loaded from disk at construction time
   */
  [[nodiscard]] uint32_t getEntryCount() const noexcept;

 private:
  std::string path_;
  bool loop_;

  std::vector<OutputItem> entries_;
  std::mutex cursor_mutex_;
  uint32_t cursor_ = 0;
  bool exhausted_ = false;
};

} // namespace beast

#endif // BEAST_PIPES_PROGRAM_STORAGE_SOURCE_PIPE_HPP_
