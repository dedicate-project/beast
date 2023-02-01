#include <beast/time_functions.hpp>

#ifdef _MSC_VER
struct tm* gmtime_r(const time_t* _clock, struct tm* _result) noexcept {
    _gmtime64_s(_result, _clock);
    return _result;
}

struct tm* localtime_r(const time_t* _clock, struct tm* _result) noexcept {
    _localtime64_s(_result, _clock);
    return _result;
}
#endif
