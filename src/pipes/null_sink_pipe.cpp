#include <beast/pipes/null_sink_pipe.hpp>

namespace beast {

NullSinkPipe::NullSinkPipe() : Pipe(1) {}

void NullSinkPipe::execute() {
  while (getInputSlotAmount(0) > 0) {
    static_cast<void>(drawInput(0));
  }
}

} // namespace beast
