#include "numerics.hpp"

#include <cmath>
#include <stdlib.h>

#include "Assert.hpp"
#include "Log.hpp"

#if defined(G_MATH_OVERFLOW_CHECK)

//
// Use an integer 64 to see if the value converted is overflowing.
//
#define GMathConvert(val, max, type, func)                                    \
    i64 valueContainer = (i64)func(value);                                      \
    if (abs(valueContainer) > max)                                              \
        GPrint("Overflow converting values %llu, %llu\n", valueContainer, max); \
    const type v = (type)valueContainer;

#define GMathFunc_f32(max, type, func)           \
    type func##type(f32 value)                      \
    {                                               \
        GMathConvert(value, max, type, func##f); \
        return v;                                   \
    }

#define GMathFunc_f64(max, type, func)        \
    type func##type(f64 value)                   \
    {                                            \
        GMathConvert(value, max, type, func); \
        return v;                                \
    }

#else
#define GMathConvert(val, max, type, func) \
    (type) func(value);

#define GMathFunc_f32(max, type, func)                  \
    type func##type(f32 value)                             \
    {                                                      \
        return GMathConvert(value, max, type, func##f); \
    }

#define GMathFunc_f64(max, type, func)               \
    type func##type(f64 value)                          \
    {                                                   \
        return GMathConvert(value, max, type, func); \
    }

#endif // RAPTOR_MATH_OVERFLOW_CHECK

//
// Avoid double typeing functions for float and double
//
#define GMathFunc_f32_f64(max, type, func) \
    GMathFunc_f32(max, type, func)         \
    GMathFunc_f64(max, type, func)

// Function declarations //////////////////////////////////////////////////////////////////////////

// Ceil
GMathFunc_f32_f64(UINT32_MAX, u32, ceil)
GMathFunc_f32_f64(UINT16_MAX, u16, ceil)
GMathFunc_f32_f64(INT32_MAX, i32, ceil)
GMathFunc_f32_f64(INT16_MAX, i16, ceil)

// Floor
GMathFunc_f32_f64(UINT32_MAX, u32, floor)
GMathFunc_f32_f64(UINT16_MAX, u16, floor)
GMathFunc_f32_f64(INT32_MAX, i32, floor)
GMathFunc_f32_f64(INT16_MAX, i16, floor)

// Round
GMathFunc_f32_f64(UINT32_MAX, u32, round)
GMathFunc_f32_f64(UINT16_MAX, u16, round)
GMathFunc_f32_f64(INT32_MAX, i32, round)
GMathFunc_f32_f64(INT16_MAX, i16, round)

f32 GetRandomValue(f32 min, f32 max)
{
    GASSERT(min < max);

    f32 rnd = (f32)rand() / (f32)RAND_MAX;
    rnd = (max - min) * rnd + min;
    return rnd;
}

