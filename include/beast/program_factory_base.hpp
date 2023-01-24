#ifndef BEAST_PROGRAM_FACTORY_BASE_HPP_
#define BEAST_PROGRAM_FACTORY_BASE_HPP_

// Internal
#include <beast/program.hpp>

namespace beast {

/**
 * @class ProgramFactoryBase
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Implement this function.
 */
class ProgramFactoryBase {
 public:
  /**
   * @fn ProgramFactoryBase::generate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Implement this function.
   *
   * @param size tbd
   * @return tbd
   */
  virtual Program generate(uint32_t size) = 0;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_FACTORY_BASE_HPP_
