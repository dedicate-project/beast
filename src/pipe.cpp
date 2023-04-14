#include <beast/pipe.hpp>

// Standard
#include <stdexcept>

namespace beast {

Pipe::Pipe(uint32_t max_candidates, uint32_t input_slots, uint32_t output_slots)
    : max_candidates_{max_candidates} {
  inputs_.resize(input_slots);
  outputs_.resize(output_slots);
}

void Pipe::addInput(uint32_t slot_index, const std::vector<unsigned char>& candidate) {
  std::scoped_lock lock(inputs_mutex_);
  inputs_[slot_index].push_back(candidate);
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

  std::vector<unsigned char> item = inputs_[slot_index].front();
  inputs_[slot_index].pop_front();

  return item;
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
