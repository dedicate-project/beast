#ifndef BEAST_TIME_FUNCTIONS_HPP_
#define BEAST_TIME_FUNCTIONS_HPP_

#ifndef _MSC_VER
#include <ctime>
#endif
#ifdef _MSC_VER
#include <time.h>
#endif

#ifdef _MSC_VER
/* These two functions are implemented here for compatibility with time.h on MSVC builds. The reason
   is that localtime_r is not implemented for MSVC and would break the build. This implementation
   should be threadsafe and complement in time.h what is missing relative to ctime. */
struct tm* gmtime_r(const time_t* _clock, struct tm* _result);
struct tm* localtime_r(const time_t* _clock, struct tm* _result);
#endif

#endif  // BEAST_TIME_FUNCTIONS_HPP_
