#include <beast/pipe.hpp>

// Standard
#include <stdexcept>

namespace beast {

Pipe::Pipe(uint32_t max_candidates)
  : max_candidates_{max_candidates} {
}

void Pipe::addInput(const std::vector<unsigned char>& candidate) {
  input_.push_back(candidate);
}

bool Pipe::hasSpace() const {
  return input_.size() < max_candidates_;
}

std::vector<unsigned char> Pipe::drawInput(uint32_t /*slot_index*/) {
  if (input_.empty()) {
    throw std::underflow_error("No input candidates available to draw.");
  }

  std::vector<unsigned char> item = input_.front();
  input_.pop_front();

  return item;
}

bool Pipe::hasOutput(uint32_t /*slot_index*/) const {
  return !output_.empty();
}

Pipe::OutputItem Pipe::drawOutput(uint32_t /*slot_index*/) {
  if (output_.empty()) {
    throw std::underflow_error("No output candidates available to draw.");
  }

  OutputItem item = output_.front();
  output_.pop_front();

  return item;
}

} // namespace beast
