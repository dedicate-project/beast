#ifndef BEAST_VERSION_HPP_
#define BEAST_VERSION_HPP_

// Standard
#include <array>
#include <cstdint>
#include <string>

// Internal
#include <beast/version.h>

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

/**
 * @brief Returns the version of the BEAST library as a string in the format "MAJOR.MINOR.PATCH".
 *
 * @return A string representation of the version of the BEAST library.
 */
[[nodiscard]] std::string getVersionString() noexcept;

} // namespace beast

#endif // BEAST_VERSION_HPP_
