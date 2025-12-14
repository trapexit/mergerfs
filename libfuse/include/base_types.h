#pragma once

#include <stdint.h>

typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;

typedef const uint32_t cu32;
typedef const int32_t  cs32;
typedef const uint64_t cu64;
typedef const int64_t  cs64;

typedef float       f32;
typedef double      f64;
typedef long double f80;
typedef char assertion_f32_is_32bit[sizeof(f32) == 4 ? 1 : -1];
typedef char assertion_f64_is_64bit[sizeof(f64) == 8 ? 1 : -1];
typedef char assertion_f80_is_at_least_64bit[sizeof(f64) >= 8 ? 1 : -1];

typedef const float       cf32;
typedef const double      cf64;
typedef const long double cf80;
