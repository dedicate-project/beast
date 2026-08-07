#include <beast/pipes/verification_sink_pipe.hpp>

// Standard
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

// Third-party
#include <nlohmann/json.hpp>

// Internal
#include <beast/program.hpp>
#include <beast/vm_session.hpp>

namespace beast {

namespace {
constexpr uint32_t kDefaultTopK = 10;

bool betterEntry(const VerificationSinkPipe::Entry& a, const VerificationSinkPipe::Entry& b) {
  if (a.success_rate != b.success_rate) {
    return a.success_rate > b.success_rate;
  }
  return a.mean_score > b.mean_score;
}
}  // namespace

VerificationSinkPipe::VerificationSinkPipe(uint32_t max_candidates, std::string path,
                                           uint32_t top_k, uint32_t verify_worlds,
                                           uint32_t memory_variables,
                                           uint32_t string_table_items,
                                           uint32_t string_table_item_length,
                                           TaskWorldEvaluator::Config config)
    : Pipe(max_candidates, /*input_slots=*/1, /*output_slots=*/0),
      path_(std::move(path)),
      top_k_(top_k == 0 ? kDefaultTopK : top_k),
      verify_worlds_(verify_worlds == 0 ? 1U : verify_worlds),
      memory_variables_(memory_variables),
      string_table_items_(string_table_items),
      string_table_item_length_(string_table_item_length),
      evaluator_(std::move(config)) {}

bool VerificationSinkPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

void VerificationSinkPipe::execute() {
  bool changed = false;
  while (getInputSlotAmount(0) > 0) {
    const auto item = drawInputWithScore(0);
    const Entry entry = verifyProgram(item.data, item.score);
    if (foldEntry(entry)) {
      changed = true;
    }
  }
  if (changed && !path_.empty()) {
    std::scoped_lock lock(entries_mutex_);
    persistLocked();
  }
}

VerificationSinkPipe::Entry VerificationSinkPipe::verifyProgram(
    const std::vector<unsigned char>& data, double train_score) const {
  Entry entry;
  entry.data = data;
  entry.train_score = train_score;

  uint32_t successes = 0;
  double score_sum = 0.0;
  double items_sum = 0.0;
  for (uint32_t k = 0; k < verify_worlds_; ++k) {
    const uint64_t seed = evaluator_.enumeratedPoolSeed(k);
    VmSession session(Program(data), memory_variables_, string_table_items_,
                      string_table_item_length_);
    const auto result = evaluator_.runEpisode(session, seed);
    score_sum += result.score;
    if (result.task_complete) {
      ++successes;
    }
    if (result.items_total > 0) {
      items_sum += static_cast<double>(result.items_collected) / result.items_total;
    } else if (result.reached_goal) {
      items_sum += 1.0;
    }
  }
  entry.success_rate = static_cast<double>(successes) / verify_worlds_;
  entry.mean_score = score_sum / verify_worlds_;
  entry.mean_items = items_sum / verify_worlds_;
  return entry;
}

bool VerificationSinkPipe::foldEntry(const Entry& entry) {
  std::scoped_lock lock(entries_mutex_);
  if (entries_.size() >= top_k_ && !betterEntry(entry, entries_.back())) {
    return false;
  }
  for (const auto& existing : entries_) {
    if (existing.data == entry.data) {
      return false;
    }
  }
  entries_.push_back(entry);
  std::sort(entries_.begin(), entries_.end(), betterEntry);
  if (entries_.size() > top_k_) {
    entries_.resize(top_k_);
  }
  return true;
}

void VerificationSinkPipe::persistLocked() const {
  const auto& config = evaluator_.getConfig();
  nlohmann::json doc;
  doc["verify_worlds"] = verify_worlds_;
  doc["pool"]["modulus"] = config.pool_modulus;
  doc["pool"]["residues"] = config.pool_residues;
  nlohmann::json entries = nlohmann::json::array();
  for (const auto& entry : entries_) {
    entries.push_back({{"success_rate", entry.success_rate},
                       {"mean_score", entry.mean_score},
                       {"mean_items", entry.mean_items},
                       {"train_score", entry.train_score},
                       {"generalization_gap", entry.train_score - entry.mean_score},
                       {"data", entry.data}});
  }
  doc["entries"] = std::move(entries);

  const std::filesystem::path final_path(path_);
  const std::filesystem::path parent = final_path.parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }
  const std::filesystem::path tmp_path = final_path.string() + ".tmp";
  {
    std::ofstream out(tmp_path);
    if (!out) {
      return;
    }
    out << doc.dump(2);
  }
  std::error_code rename_ec;
  std::filesystem::rename(tmp_path, final_path, rename_ec);
  if (rename_ec) {
    std::error_code copy_ec;
    std::filesystem::copy_file(tmp_path, final_path,
                               std::filesystem::copy_options::overwrite_existing, copy_ec);
    std::filesystem::remove(tmp_path, copy_ec);
  }
}

std::vector<VerificationSinkPipe::Entry> VerificationSinkPipe::getEntries() const {
  std::scoped_lock lock(entries_mutex_);
  return entries_;
}

}  // namespace beast
