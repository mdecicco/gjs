#pragma once
#include <stdint.h>

#define FORCE_INLINE __forceinline

namespace gjs {
    typedef uint64_t    u64;
    typedef int64_t        i64;
    typedef uint32_t    u32;
    typedef int32_t        i32;
    typedef uint16_t    u16;
    typedef int16_t        i16;
    typedef uint8_t        u8;
    typedef int8_t        i8;
    typedef float        f32;
    typedef double        f64;

    typedef u64            address;
    typedef f64            decimal;
    typedef i64            integer;
    typedef u64            uinteger;
};
