#include <beast/pipes/program_storage_sink_pipe.hpp>

// Standard
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

// Third-party
#include <nlohmann/json.hpp>

namespace beast {

namespace {
constexpr uint32_t kDefaultTopK = 10;
} // namespace

ProgramStorageSinkPipe::ProgramStorageSinkPipe(uint32_t max_candidates, std::string path,
                                               uint32_t top_k)
    : Pipe(max_candidates, /*input_slots=*/1, /*output_slots=*/0),
      path_(std::move(path)),
      top_k_(top_k == 0 ? kDefaultTopK : top_k) {
  // Eagerly load any pre-existing ledger so subsequent inserts merge with it instead
  // of replacing it. Tolerate missing / malformed files -- the sink is supposed to be
  // "best effort" persistence, not a hard dependency for the pipeline to run.
  if (path_.empty()) {
    return;
  }
  std::ifstream in(path_);
  if (!in) {
    return;
  }
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
      entries_.push_back(std::move(item));
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const OutputItem& a, const OutputItem& b) { return a.score > b.score; });
    if (entries_.size() > top_k_) {
      entries_.resize(top_k_);
    }
  } catch (const std::exception&) {
    // Best-effort: ignore corrupted files. The first successful flush will overwrite
    // the on-disk copy with a well-formed one.
    entries_.clear();
  }
}

bool ProgramStorageSinkPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

void ProgramStorageSinkPipe::execute() {
  bool changed = false;
  while (getInputSlotAmount(0) > 0) {
    auto item = drawInputWithScore(0);
    if (foldCandidate(item)) {
      changed = true;
    }
  }
  if (changed && !path_.empty()) {
    std::scoped_lock lock(entries_mutex_);
    persistLocked();
  }
}

bool ProgramStorageSinkPipe::foldCandidate(const OutputItem& candidate) {
  std::scoped_lock lock(entries_mutex_);
  // If we're already at capacity and the candidate doesn't beat the worst kept entry,
  // skip the work entirely -- this is the hot path during long stable training runs.
  if (entries_.size() >= top_k_ && candidate.score <= entries_.back().score) {
    return false;
  }
  // Suppress exact duplicates so e.g. an elitism-carrier doesn't drown the ledger with
  // many identical copies of the same genome. Two entries with the same score AND the
  // same bytes are treated as duplicates.
  for (const auto& existing : entries_) {
    if (existing.score == candidate.score && existing.data == candidate.data) {
      return false;
    }
  }
  entries_.push_back(candidate);
  std::sort(entries_.begin(), entries_.end(),
            [](const OutputItem& a, const OutputItem& b) { return a.score > b.score; });
  if (entries_.size() > top_k_) {
    entries_.resize(top_k_);
  }
  return true;
}

void ProgramStorageSinkPipe::persistLocked() const {
  // Write to a sibling temp file and rename so a concurrent reader (e.g. a
  // ProgramStorageSourcePipe pointed at the same path) never sees a half-written file.
  nlohmann::json doc = nlohmann::json::array();
  for (const auto& entry : entries_) {
    doc.push_back({{"score", entry.score}, {"data", entry.data}});
  }
  const std::filesystem::path final_path(path_);
  std::filesystem::path parent = final_path.parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    // Don't throw if directory creation fails -- the subsequent ofstream will surface
    // a more actionable error if the path is truly unusable.
  }
  const std::filesystem::path tmp_path = final_path.string() + ".tmp";
  {
    std::ofstream out(tmp_path);
    if (!out) {
      return; // best-effort; next tick will retry
    }
    out << doc.dump(2);
  }
  std::error_code rename_ec;
  std::filesystem::rename(tmp_path, final_path, rename_ec);
  if (rename_ec) {
    // Fall back to a non-atomic copy+remove. Better to leave a slightly-stale ledger
    // than to fail silently.
    std::error_code copy_ec;
    std::filesystem::copy_file(tmp_path, final_path,
                               std::filesystem::copy_options::overwrite_existing, copy_ec);
    std::filesystem::remove(tmp_path, copy_ec);
  }
}

const std::string& ProgramStorageSinkPipe::getPath() const noexcept { return path_; }

uint32_t ProgramStorageSinkPipe::getTopK() const noexcept { return top_k_; }

std::vector<Pipe::OutputItem> ProgramStorageSinkPipe::getEntries() const {
  std::scoped_lock lock(entries_mutex_);
  return entries_;
}

} // namespace beast
