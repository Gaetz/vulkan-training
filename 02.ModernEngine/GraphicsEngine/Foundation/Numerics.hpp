#pragma once

#include "Platform.hpp"


// Math utils /////////////////////////////////////////////////////////////////////////////////
// Conversions from float/double to int/uint
//
// Define this macro to check if converted value can be contained in the destination int/uint.
#define G_MATH_OVERFLOW_CHECK

// Undefine the macro versions of this.
#undef Max
#undef Min

template <typename T>
T Max(const T& a, const T& b)
{
    return a > b ? a : b;
}

template <typename T>
T Min(const T& a, const T& b) { return a < b ? a : b; }

template <typename T>
T Clamp(const T& v, const T& a, const T& b) { return v < a ? a : (v > b ? b : v); }

template <typename To, typename From>
To SafeCast(From a)
{
    To result = (To)a;

    From check = (From)result;
    GASSERT(check == result);

    return result;
}

u32 ceilu32(f32 value);
u32 ceilu32(f64 value);
u16 ceilu16(f32 value);
u16 ceilu16(f64 value);
i32 ceili32(f32 value);
i32 ceili32(f64 value);
i16 ceili16(f32 value);
i16 ceili16(f64 value);

u32 flooru32(f32 value);
u32 flooru32(f64 value);
u16 flooru16(f32 value);
u16 flooru16(f64 value);
i32 floori32(f32 value);
i32 floori32(f64 value);
i16 floori16(f32 value);
i16 floori16(f64 value);

u32 roundu32(f32 value);
u32 roundu32(f64 value);
u16 roundu16(f32 value);
u16 roundu16(f64 value);
i32 roundi32(f32 value);
i32 roundi32(f64 value);
i16 roundi16(f32 value);
i16 roundi16(f64 value);

f32 GetRandomValue(f32 min, f32 max);

const f32 gPi = 3.1415926538f;
const f32 gPi2 = 1.57079632679f;
