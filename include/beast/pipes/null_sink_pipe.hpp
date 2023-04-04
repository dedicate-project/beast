#ifndef BEAST_NULL_SINK_PIPE_HPP_
#define BEAST_NULL_SINK_PIPE_HPP_

// Standard
#include <deque>
#include <memory>
#include <stdint.h>
#include <vector>

// Internal
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class NullSinkPipe
 * @brief Receives and discards candidate programs.
 *
 * TODO(fairlight1337): Document this class.
 *
 * @author Jan Winkler
 * @date 2023-04-03
 */
class NullSinkPipe : public Pipe {
 public:
  explicit NullSinkPipe();

  void execute() override;

  uint32_t getOutputSlotCount() const override { return 0; }
};

} // namespace beast

#endif // BEAST_NULL_SINK_PIPE_HPP_
