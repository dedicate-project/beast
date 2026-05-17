#include <beast/pipes/program_storage_source_pipe.hpp>

// Standard
#include <fstream>
#include <stdexcept>
#include <utility>

// Third-party
#include <nlohmann/json.hpp>

namespace beast {

ProgramStorageSourcePipe::ProgramStorageSourcePipe(uint32_t max_candidates, std::string path,
                                                   bool loop)
    : Pipe(max_candidates, /*input_slots=*/0, /*output_slots=*/1),
      path_(std::move(path)),
      loop_(loop) {
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
  } catch (const std::exception&) {
    // Best-effort: ignore corrupted files. The pipe stays empty in that case.
    entries_.clear();
  }
}

void ProgramStorageSourcePipe::execute() {
  if (entries_.empty()) {
    return;
  }
  std::scoped_lock lock(cursor_mutex_);
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

const std::string& ProgramStorageSourcePipe::getPath() const noexcept { return path_; }

bool ProgramStorageSourcePipe::getLoop() const noexcept { return loop_; }

uint32_t ProgramStorageSourcePipe::getEntryCount() const noexcept {
  return static_cast<uint32_t>(entries_.size());
}

} // namespace beast
