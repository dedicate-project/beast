#include <beast/pipes/program_storage_source_pipe.hpp>

// Standard
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

// Third-party
#include <nlohmann/json.hpp>

namespace beast {

namespace {
/// How long to wait between stat() calls on the source ledger. Keeps the syscall cost
/// bounded when the source is in a hot empty spin (file missing or empty, worker
/// calls execute() at MHz because outputs never saturate). 500 ms is short enough to
/// pick up a freshly-written ledger before the user notices and long enough to make
/// the stat() cost invisible.
constexpr std::chrono::milliseconds kRefreshInterval{500};
} // namespace

ProgramStorageSourcePipe::ProgramStorageSourcePipe(uint32_t max_candidates, std::string path,
                                                   bool loop)
    : Pipe(max_candidates, /*input_slots=*/0, /*output_slots=*/1),
      path_(std::move(path)),
      loop_(loop) {
  if (path_.empty()) {
    return;
  }
  // Best-effort initial load. If the file isn't there yet (typical when this source is
  // wired to read a ledger that an upstream sink will populate later), reloadFromDiskLocked
  // just returns silently and we wait for the periodic refresh to pick it up.
  std::scoped_lock lock(entries_mutex_);
  reloadFromDiskLocked();
}

void ProgramStorageSourcePipe::execute() {
  std::scoped_lock lock(entries_mutex_);
  maybeRefreshFromDiskLocked();
  if (entries_.empty()) {
    return;
  }
  if (exhausted_) {
    return;
  }
  while (!outputsAreSaturated()) {
    if (cursor_ >= entries_.size()) {
      if (!loop_) {
        exhausted_ = true;
        return;
      }
      cursor_ = 0;
    }
    storeOutput(0, entries_[cursor_]);
    ++cursor_;
  }
}

void ProgramStorageSourcePipe::maybeRefreshFromDiskLocked() {
  if (path_.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  if (last_check_.time_since_epoch().count() != 0 && now - last_check_ < kRefreshInterval) {
    return;
  }
  last_check_ = now;

  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(path_, ec);
  if (ec) {
    return; // file missing / unreadable; keep whatever we already have cached
  }
  if (mtime == last_loaded_mtime_) {
    return; // file unchanged since last successful load
  }
  reloadFromDiskLocked();
}

void ProgramStorageSourcePipe::reloadFromDiskLocked() {
  std::ifstream in(path_);
  if (!in) {
    return;
  }

  std::vector<OutputItem> new_entries;
  try {
    nlohmann::json doc;
    in >> doc;
    if (!doc.is_array()) {
      return;
    }
    for (const auto& entry : doc) {
      if (!entry.contains("data") || !entry.contains("score")) {
        continue;
      }
      OutputItem item;
      item.score = entry["score"].get<double>();
      item.data = entry["data"].get<std::vector<unsigned char>>();
      new_entries.push_back(std::move(item));
    }
  } catch (const std::exception&) {
    // Best-effort: a corrupted file (mid-write race the rename in the sink missed,
    // someone editing the ledger by hand, etc.) is silently ignored. The previously
    // cached entries stay in place so we don't regress when the file recovers.
    return;
  }

  entries_ = std::move(new_entries);
  // Restart from the top of the fresh batch -- if Stage A just promoted a new top
  // genome, Stage B should see it first, not whatever stale position the cursor
  // happened to be at.
  cursor_ = 0;
  // A fresh write counts as "new material", which un-sticks the source even if it had
  // previously exhausted a static one-shot ledger.
  exhausted_ = false;
  std::error_code ec;
  last_loaded_mtime_ = std::filesystem::last_write_time(path_, ec);
}

const std::string& ProgramStorageSourcePipe::getPath() const noexcept { return path_; }

bool ProgramStorageSourcePipe::getLoop() const noexcept { return loop_; }

uint32_t ProgramStorageSourcePipe::getEntryCount() const noexcept {
  std::scoped_lock lock(entries_mutex_);
  return static_cast<uint32_t>(entries_.size());
}

} // namespace beast
