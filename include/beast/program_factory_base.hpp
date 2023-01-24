#ifndef BEAST_PROGRAM_FACTORY_BASE_HPP_
#define BEAST_PROGRAM_FACTORY_BASE_HPP_

// Internal
#include <beast/program.hpp>

namespace beast {

/**
 * @class ProgramFactoryBase
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this class.
 */
class ProgramFactoryBase {
 public:
  /**
   * @fn ProgramFactoryBase::~ProgramFactoryBase
   * @brief Default virtual destructor
   *
   * Ensures vtable consistency.
   */
  virtual ~ProgramFactoryBase() = default;

  /**
   * @fn ProgramFactoryBase::generate
   * @brief Generates a program based on the factory's semantics
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param size The size of the program to generate
   * @return The generated program
   */
  virtual Program generate(
      uint32_t size, uint32_t memory_size, uint32_t string_table_size,
      uint32_t string_table_item_length) = 0;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_FACTORY_BASE_HPP_
