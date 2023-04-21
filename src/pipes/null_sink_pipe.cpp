#include <beast/pipes/null_sink_pipe.hpp>

namespace beast {

NullSinkPipe::NullSinkPipe(uint32_t max_candidates) : Pipe(max_candidates, 1, 0) {}

void NullSinkPipe::execute() {
  while (getInputSlotAmount(0) > 0) {
    static_cast<void>(drawInput(0));
  }
}

} // namespace beast
