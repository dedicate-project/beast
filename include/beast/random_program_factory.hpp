#ifndef BEAST_RANDOM_PROGRAM_FACTORY_HPP_
#define BEAST_RANDOM_PROGRAM_FACTORY_HPP_

// Internal
#include <beast/program_factory_base.hpp>

namespace beast {

/**
 * @class RandomProgramFactory
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Implement this function.
 */
class RandomProgramFactory : public ProgramFactoryBase {
 public:
  /**
   * @fn RandomProgramFactory::generate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Implement this function.
   *
   * @param size tbd
   * @return tbd
   */
  Program generate(size_t size) override;
};

}  // namespace beast

#endif  // BEAST_RANDOM_PROGRAM_FACTORY_HPP_
