#ifndef BEAST_PIPE_HPP_
#define BEAST_PIPE_HPP_

// Standard
#include <stdint.h>
#include <vector>

// Internal
#include <beast/vm_session.hpp>

namespace beast {

/**
 * @class Pipe
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this class.
 */
class Pipe {
 public:
  /**
   * @class Pipe::Pipe
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  Pipe(uint32_t max_size, uint32_t item_size);

  /**
   * @class Pipe::~Pipe
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  virtual ~Pipe() = default;

  /**
   * @class Pipe::addIndividualToInitialPopulation
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  void addInput(VmSession& session);

  /**
   * @class Pipe::evolve
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  void evolve();

  /**
   * @class Pipe::hasSpace
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  bool hasSpace();

private:
  /**
   * @var Pipe::max_size_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  uint32_t max_size_;

  /**
   * @var Pipe::item_size_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  uint32_t item_size_;

  /**
   * @var Pipe::input_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  std::vector<VmSession> input_;
};

}  // namespace beast

#endif  // BEAST_PIPE_HPP_
