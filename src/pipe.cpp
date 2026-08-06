#include <beast/pipe.hpp>

// Standard
#include <stdexcept>

namespace beast {

Pipe::Pipe(uint32_t max_candidates, uint32_t input_slots, uint32_t output_slots)
    : max_candidates_{max_candidates} {
  inputs_.resize(input_slots);
  outputs_.resize(output_slots);
}

void Pipe::setStopToken(std::shared_ptr<std::atomic<bool>> token) noexcept {
  // Atomic shared_ptr replacement is not portable across all stdlibs we ship to (libstdc++
  // exposes `std::atomic<std::shared_ptr<T>>` only from C++20 onwards and even then with
  // caveats). The token is set exactly once per `Pipeline::start()` and cleared once per
  // stop(); both happen on the controlling thread while no worker is running the pipe (the
  // pipe is only added to a Pipeline before its first start() call). Plain assignment is
  // therefore race-free in practice. `noexcept` because shared_ptr's move assignment is
  // noexcept and we don't allocate.
  stop_token_ = std::move(token);
}

bool Pipe::isStopRequested() const noexcept {
  // Hot path: called per VM step in expensive evaluators and per genome in the GA wrapper.
  // The shared_ptr is read on the calling thread (which set it) so no synchronisation is
  // needed for the pointer itself; the load through the pointer is relaxed because the only
  // consumer is a fast-path "should I bail?" check -- correctness doesn't depend on seeing
  // the latest store the very nanosecond it lands, only on seeing it eventually.
  const auto token = stop_token_; // copy the shared_ptr so a concurrent setStopToken is safe
  if (!token) {
    return false;
  }
  return token->load(std::memory_order_relaxed);
}

std::shared_ptr<std::atomic<bool>> Pipe::getStopToken() const noexcept {
  return stop_token_;
}

void Pipe::addInput(uint32_t slot_index, const std::vector<unsigned char>& candidate) {
  std::scoped_lock lock(inputs_mutex_);
  // Wrap as an OutputItem with score 0.0; callers that have a real score should use
  // addInputWithScore. We keep this signature for backwards-compatible external usage
  // (initialization, tests) where there is no meaningful score.
  inputs_[slot_index].push_back(OutputItem{candidate, 0.0});
}

void Pipe::addInputWithScore(uint32_t slot_index, const OutputItem& candidate) {
  std::scoped_lock lock(inputs_mutex_);
  inputs_[slot_index].push_back(candidate);
}

void Pipe::addInputWithScore(uint32_t slot_index, OutputItem&& candidate) {
  std::scoped_lock lock(inputs_mutex_);
  inputs_[slot_index].push_back(std::move(candidate));
}

bool Pipe::inputHasSpace(uint32_t slot_index) {
  std::scoped_lock lock(inputs_mutex_);
  return inputs_[slot_index].size() < max_candidates_;
}

std::vector<unsigned char> Pipe::drawInput(uint32_t slot_index) {
  std::scoped_lock lock(inputs_mutex_);
  if (inputs_[slot_index].empty()) {
    throw std::underflow_error("No input candidates available to draw.");
  }

  // Score is intentionally dropped here; callers that need it should use
  // drawInputWithScore. Moving out of the OutputItem first lets us return the bytes
  // without an extra copy.
  std::vector<unsigned char> item = std::move(inputs_[slot_index].front().data);
  inputs_[slot_index].pop_front();

  return item;
}

Pipe::OutputItem Pipe::drawInputWithScore(uint32_t slot_index) {
  std::scoped_lock lock(inputs_mutex_);
  if (inputs_[slot_index].empty()) {
    throw std::underflow_error("No input candidates available to draw.");
  }

  OutputItem item = std::move(inputs_[slot_index].front());
  inputs_[slot_index].pop_front();
  return item;
}

double Pipe::peekInputScore(uint32_t slot_index) {
  std::scoped_lock lock(inputs_mutex_);
  if (inputs_[slot_index].empty()) {
    throw std::underflow_error("No input candidates available to peek.");
  }
  return inputs_[slot_index].front().score;
}

bool Pipe::hasOutput(uint32_t slot_index) {
  std::scoped_lock lock(outputs_mutex_);
  return !outputs_[slot_index].empty();
}

Pipe::OutputItem Pipe::drawOutput(uint32_t slot_index) {
  std::scoped_lock lock(outputs_mutex_);
  if (outputs_[slot_index].empty()) {
    throw std::underflow_error("No output candidates available to draw.");
  }

  OutputItem item = std::move(outputs_[slot_index].front());
  outputs_[slot_index].erase(outputs_[slot_index].begin());

  return item;
}

uint32_t Pipe::getInputSlotAmount(uint32_t slot_index) {
  std::scoped_lock lock(inputs_mutex_);
  return static_cast<uint32_t>(inputs_[slot_index].size());
}

uint32_t Pipe::getOutputSlotAmount(uint32_t slot_index) {
  std::scoped_lock lock(outputs_mutex_);
  return static_cast<uint32_t>(outputs_[slot_index].size());
}

uint32_t Pipe::getInputSlotCount() const { return static_cast<uint32_t>(inputs_.size()); }

uint32_t Pipe::getOutputSlotCount() const { return static_cast<uint32_t>(outputs_.size()); }

uint32_t Pipe::getMaxCandidates() const { return max_candidates_; }

bool Pipe::inputsAreSaturated() {
  std::scoped_lock lock(inputs_mutex_);
  for (uint32_t idx = 0; idx < getInputSlotCount(); ++idx) {
    if (static_cast<uint32_t>(inputs_[idx].size()) < max_candidates_) {
      return false;
    }
  }
  return true;
}

bool Pipe::outputsAreSaturated() {
  const uint32_t outputSlotCount = getOutputSlotCount();
  if (outputSlotCount == 0) {
    return false;
  }
  std::scoped_lock lock(outputs_mutex_);
  for (uint32_t idx = 0; idx < outputSlotCount; ++idx) {
    if (static_cast<uint32_t>(outputs_[idx].size()) < max_candidates_) {
      return false;
    }
  }
  return true;
}

void Pipe::storeOutput(uint32_t slot_index, const OutputItem& output) {
  std::scoped_lock lock(outputs_mutex_);
  if (slot_index >= outputs_.size()) {
    throw std::invalid_argument("Slot index for output too large");
  }
  outputs_[slot_index].push_back(output);
}

void Pipe::storeOutput(uint32_t slot_index, OutputItem&& output) {
  std::scoped_lock lock(outputs_mutex_);
  if (slot_index >= outputs_.size()) {
    throw std::invalid_argument("Slot index for output too large");
  }
  outputs_[slot_index].push_back(std::move(output));
}

} // namespace beast
