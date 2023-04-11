#ifndef BEAST_TIME_FUNCTIONS_HPP_
#define BEAST_TIME_FUNCTIONS_HPP_

#ifndef _MSC_VER
#include <ctime>
#endif
#ifdef _MSC_VER
#include <time.h>
#endif

#ifdef _MSC_VER
/**
 * @brief This function is implemented here for compatibility with time.h on MSVC builds.
 * The reason is that localtime_r is not implemented for MSVC and would break the build.
 * This implementation should be threadsafe and complement in time.h what is missing relative to
 * ctime.
 *
 * @param _clock Pointer to a time_t object representing the time to convert
 * @param _result Pointer to a tm struct to hold the converted time
 * @return A pointer to the tm struct holding the converted time
 */
[[nodiscard]] struct tm* gmtime_r(const time_t* _clock, struct tm* _result) noexcept;

/**
 * @brief This function is implemented here for compatibility with time.h on MSVC builds.
 * The reason is that localtime_r is not implemented for MSVC and would break the build.
 * This implementation should be threadsafe and complement in time.h what is missing relative to
 * ctime.
 *
 * @param _clock Pointer to a time_t object representing the time to convert
 * @param _result Pointer to a tm struct to hold the converted time
 * @return A pointer to the tm struct holding the converted time
 */
[[nodiscard]] struct tm* localtime_r(const time_t* _clock, struct tm* _result) noexcept;
#endif

#endif // BEAST_TIME_FUNCTIONS_HPP_
