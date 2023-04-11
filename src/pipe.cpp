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
  inputs_[slot_index].push_back(candidate);
}

bool Pipe::inputHasSpace(uint32_t slot_index) const {
  return inputs_[slot_index].size() < max_candidates_;
}

std::vector<unsigned char> Pipe::drawInput(uint32_t slot_index) {
  if (inputs_[slot_index].empty()) {
    throw std::underflow_error("No input candidates available to draw.");
  }

  std::vector<unsigned char> item = inputs_[slot_index].front();
  inputs_[slot_index].pop_front();

  return item;
}

bool Pipe::hasOutput(uint32_t slot_index) const { return !outputs_[slot_index].empty(); }

Pipe::OutputItem Pipe::drawOutput(uint32_t slot_index) {
  if (outputs_[slot_index].empty()) {
    throw std::underflow_error("No output candidates available to draw.");
  }

  OutputItem item = outputs_[slot_index].front();
  outputs_[slot_index].pop_front();

  return item;
}

uint32_t Pipe::getInputSlotAmount(uint32_t slot_index) const { return inputs_[slot_index].size(); }

uint32_t Pipe::getOutputSlotAmount(uint32_t slot_index) const {
  return outputs_[slot_index].size();
}

bool Pipe::inputsAreSaturated() const {
  for (uint32_t idx = 0; idx < getInputSlotCount(); ++idx) {
    if (getInputSlotAmount(idx) < max_candidates_) {
      return false;
    }
  }
  return true;
}

bool Pipe::outputsAreSaturated() const {
  const uint32_t outputSlotCount = getOutputSlotCount();
  if (outputSlotCount == 0) {
    return false;
  }
  for (uint32_t idx = 0; idx < outputSlotCount; ++idx) {
    if (getOutputSlotAmount(idx) < max_candidates_) {
      return false;
    }
  }
  return true;
}

} // namespace beast
