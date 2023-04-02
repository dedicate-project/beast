#ifndef BEAST_PROGRAM_FACTORY_BASE_HPP_
#define BEAST_PROGRAM_FACTORY_BASE_HPP_

// Internal
#include <beast/program.hpp>

namespace beast {

/**
 * @class ProgramFactoryBase
 * @brief Base class for program factories
 *
 * Program factories generate programs based on high level parameters and criteria. A concrete
 * implementation of a factory can have a multitude of parameterization options that define the
 * characteristics of the generated programs.
 */
class ProgramFactoryBase {
 public:
  /**
   * @fn ProgramFactoryBase::generate
   * @brief Generates a program based on the factory's semantics
   *
   * Generates a Program instance that follows the factory's concrete implementation's
   * characteristics. The given parameters that define a runtime-parameterization are used to make
   * an educated guess on how to make the generated program executable. For example, only variables
   * may be declared or used whose indices lie within the expected memory size to avoid illogical
   * program traits. The concrete use of these parameters, if any, depends on the respective
   * implementation of the factory though.
   *
   * @param size The maximum size of the program to generate, in bytes
   * @param memory_size The memory size the generated program would be executed with
   * @param string_table_size The string table size the generated program would be executed with
   * @param string_table_item_length The string table item length the generated program would be
   *        executed with
   * @return The generated Program instance
   */
  [[nodiscard]] virtual Program generate(
      uint32_t size, uint32_t memory_size, uint32_t string_table_size,
      uint32_t string_table_item_length) = 0;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_FACTORY_BASE_HPP_
