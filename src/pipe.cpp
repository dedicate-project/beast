#include <beast/pipe.hpp>

namespace beast {

Pipe::Pipe(uint32_t max_size, uint32_t item_size)
  : max_size_{max_size}, item_size_{item_size} {
}

void Pipe::addInput(VmSession& session) {
  input_.push_back(session);
}

void Pipe::evolve() {
}

bool Pipe::hasSpace() {
  return input_.size() < max_size_;
}

}  // namespace beast
