#include <beast/version.hpp>

namespace beast {

std::array<uint8_t, 3> getVersion() noexcept {
  return std::array<uint8_t, 3>{BEAST_VERSION_MAJOR, BEAST_VERSION_MINOR, BEAST_VERSION_PATCH};
}

std::string getVersionString() noexcept {
  const auto version = beast::getVersion();
  return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." + std::to_string(version[2]);
}
} // namespace beast
