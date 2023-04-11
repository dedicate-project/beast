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
 * This class represents a pipeline that discards candidate programs received as input without any
 * processing or evaluation. It is intended for use as a placeholder when implementing pipeline
 * topologies and allows for easy switching between different pipeline configurations.
 *
 * @note This class does not provide any public methods as it is intended to act as a sink in a
 * pipeline topology. Candidate programs are simply received as input and discarded.
 *
 * @author Jan Winkler
 * @date 2023-04-03
 */
class NullSinkPipe : public Pipe {
 public:
  /**
   * @brief Initializes this NullSinkPipe instance
   */
  explicit NullSinkPipe();

  /**
   * @brief Executes this NullSinkPipe instance
   *
   * This function executes this NullSinkPipe instance. As a sink, it simply receives and
   * discards any candidate programs passed to it without any processing or evaluation.
   */
  void execute() override;
};

} // namespace beast

#endif // BEAST_NULL_SINK_PIPE_HPP_
