#ifndef BEAST_BEAST_HPP_
#define BEAST_BEAST_HPP_

// Standard
#include <array>

// Internal
#include <beast/cpu_virtual_machine.hpp>
#include <beast/evaluator.hpp>
#include <beast/opcodes.hpp>
#include <beast/pipe.hpp>
#include <beast/pipeline.hpp>
#include <beast/program.hpp>
#include <beast/random_program_factory.hpp>
#include <beast/time_functions.hpp>
#include <beast/version.h>
#include <beast/vm_session.hpp>

#include <beast/evaluators/aggregation_evaluator.hpp>
#include <beast/evaluators/random_serial_data_passthrough_evaluator.hpp>
#include <beast/evaluators/operator_usage_evaluator.hpp>
#include <beast/evaluators/runtime_statistics_evaluator.hpp>

#include <beast/pipes/evaluator_pipe.hpp>

namespace beast {

/**
 * @brief Returns the version of the BEAST library
 *
 * The version returned by this function is an array containing the major, minor, and patch version
 * parts in the format {MAJOR, MINOR, PATCH} and follows the semantic versioning scheme.
 *
 * @return A 3-element array holding the major, minor, and patch version parts.
 */
[[nodiscard]] std::array<uint8_t, 3> getVersion() noexcept;

}  // namespace beast

#endif  // BEAST_BEAST_HPP_
