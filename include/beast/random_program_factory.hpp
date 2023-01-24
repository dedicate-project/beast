#ifndef BEAST_RANDOM_PROGRAM_FACTORY_HPP_
#define BEAST_RANDOM_PROGRAM_FACTORY_HPP_

// Internal
#include <beast/program_factory_base.hpp>

namespace beast {

/**
 * @class RandomProgramFactory
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this class.
 */
class RandomProgramFactory : public ProgramFactoryBase {
 public:
  /**
   * @fn ProgramFactoryBase::~RandomProgramFactory
   * @brief Default virtual destructor
   *
   * Ensures vtable consistency.
   */
  virtual ~RandomProgramFactory() = default;

  /**
   * @fn RandomProgramFactory::generate
   * @brief Generates a program consisting of random but valid operators and operands
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param size The size of the program to generate
   * @return A randomly generated program
   */
  Program generate(
      uint32_t size, uint32_t memory_size, uint32_t string_table_size,
      uint32_t string_table_item_length) override;
};

}  // namespace beast

#endif  // BEAST_RANDOM_PROGRAM_FACTORY_HPP_
