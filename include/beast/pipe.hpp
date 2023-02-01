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
  Pipe(uint32_t population_size, uint32_t individual_size);

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
  void addIndividualToInitialPopulation(VmSession& session);

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
   * @var Pipe::population_size_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  uint32_t population_size_;

  /**
   * @var Pipe::individual_size_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  uint32_t individual_size_;

  /**
   * @var Pipe::initial_population_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  std::vector<VmSession> initial_population_;
};

}  // namespace beast

#endif  // BEAST_PIPE_HPP_
