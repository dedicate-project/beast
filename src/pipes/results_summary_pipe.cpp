#include <beast/pipes/results_summary_pipe.hpp>

// Standard
#include <algorithm>
#include <numeric>
#include <utility>

namespace beast {

namespace {
constexpr uint32_t kDefaultWindowSize = 256;
} // namespace

ResultsSummaryPipe::ResultsSummaryPipe(uint32_t max_candidates, uint32_t window_size)
    : Pipe(max_candidates, 1, 1),
      window_size_(window_size == 0 ? kDefaultWindowSize : window_size) {}

bool ResultsSummaryPipe::inputsAreSaturated() { return getInputSlotAmount(0) > 0; }

void ResultsSummaryPipe::execute() {
  // Drain the input slot all the way down each tick, mirroring NullSinkPipe's "consume
  // anything available" behaviour. We stop once the output slot fills so we apply natural
  // back-pressure to upstream when downstream is slow.
  while (getInputSlotAmount(0) > 0) {
    auto item = drawInputWithScore(0);
    recordScore(item.score, item.data);
    // Always preserve the score on the way out so a chain of summary pipes downstream still
    // sees meaningful scores.
    storeOutput(0, std::move(item));
    if (outputsAreSaturated()) {
      break;
    }
  }
}

void ResultsSummaryPipe::recordScore(double score, const std::vector<unsigned char>& data) {
  std::scoped_lock lock(summary_mutex_);
  recent_scores_.push_back(score);
  while (recent_scores_.size() > window_size_) {
    recent_scores_.pop_front();
  }
  ++count_total_;
  last_score_ = score;
  if (!has_any_ || score > best_ever_score_) {
    best_ever_score_ = score;
    best_ever_data_ = data;
    has_any_ = true;
  }
}

ResultsSummaryPipe::Summary ResultsSummaryPipe::getSummary() const {
  std::scoped_lock lock(summary_mutex_);
  Summary out;
  out.count_total = count_total_;
  out.count_window = static_cast<uint64_t>(recent_scores_.size());
  out.last_score = last_score_;
  out.best_ever_score = best_ever_score_;
  out.best_ever_data = best_ever_data_;
  if (!recent_scores_.empty()) {
    const auto [min_it, max_it] = std::minmax_element(recent_scores_.begin(),
                                                      recent_scores_.end());
    out.min_score = *min_it;
    out.max_score = *max_it;
    out.mean_score =
        std::accumulate(recent_scores_.begin(), recent_scores_.end(), 0.0) /
        static_cast<double>(recent_scores_.size());
  }
  return out;
}

void ResultsSummaryPipe::resetSummary() {
  std::scoped_lock lock(summary_mutex_);
  recent_scores_.clear();
  count_total_ = 0;
  last_score_ = 0.0;
  best_ever_score_ = 0.0;
  best_ever_data_.clear();
  has_any_ = false;
}

uint32_t ResultsSummaryPipe::getWindowSize() const noexcept { return window_size_; }

ResultsSummaryPipe::PersistentState ResultsSummaryPipe::exportState() const {
  std::scoped_lock lock(summary_mutex_);
  PersistentState state;
  state.recent_scores = recent_scores_;
  state.count_total = count_total_;
  state.last_score = last_score_;
  state.best_ever_score = best_ever_score_;
  state.best_ever_data = best_ever_data_;
  state.has_any = has_any_;
  return state;
}

void ResultsSummaryPipe::importState(PersistentState state) {
  std::scoped_lock lock(summary_mutex_);
  recent_scores_ = std::move(state.recent_scores);
  // Honour the post-edit `window_size_` -- the user may have shrunk it deliberately.
  while (recent_scores_.size() > window_size_) {
    recent_scores_.pop_front();
  }
  count_total_ = state.count_total;
  last_score_ = state.last_score;
  best_ever_score_ = state.best_ever_score;
  best_ever_data_ = std::move(state.best_ever_data);
  has_any_ = state.has_any;
}

} // namespace beast
