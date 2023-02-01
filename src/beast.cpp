#include <beast/beast.hpp>

namespace beast {

std::array<uint8_t, 3> getVersion() noexcept {
  return std::array<uint8_t, 3>{BEAST_VERSION_MAJOR, BEAST_VERSION_MINOR, BEAST_VERSION_PATCH};
}

}  // namespace beast
