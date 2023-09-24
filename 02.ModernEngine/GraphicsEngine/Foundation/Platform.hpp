#pragma once

#include <stdint.h>

#if !defined(_MSC_VER)
#include <signal.h>
#endif

// Macros //

#define ArraySize(array)        ( sizeof(array)/sizeof((array)[0]) )


#if defined (_MSC_VER)
#define G_INLINE                               inline
#define G_FINLINE                              __forceinline
#define G_DEBUG_BREAK                          __debugbreak();
#define G_DISABLE_WARNING(warning_number)      __pragma( warning( disable : warning_number ) )
#define G_CONCAT_OPERATOR(x, y)                x##y
#else
#define G_INLINE                               inline
#define G_FINLINE                              always_inline
#define G_DEBUG_BREAK                          raise(SIGTRAP);
#define G_CONCAT_OPERATOR(x, y)                x y
#endif // MSVC

#define G_STRINGIZE( L )                       #L 
#define G_MAKESTRING( L )                      G_STRINGIZE( L )
#define G_CONCAT(x, y)                         G_CONCAT_OPERATOR(x, y)
#define G_LINE_STRING                          G_MAKESTRING( __LINE__ ) 
#define G_FILELINE(MESSAGE)                    __FILE__ "(" G_LINE_STRING ") : " MESSAGE

// Unique names
#define G_UNIQUE_SUFFIX(PARAM)                 G_CONCAT(PARAM, __LINE__ )


// Native types typedefs /////////////////////////////////////////////////
typedef uint8_t		u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef float       f32;
typedef double      f64;

typedef size_t      sizet;

typedef const char* cstring;

static const u64 u64Max = UINT64_MAX;
static const i64 i64Max = INT64_MAX;
static const u32 u32Max = UINT32_MAX;
static const i32 i32Max = INT32_MAX;
static const u16 u16Max = UINT16_MAX;
static const i16 i16Max = INT16_MAX;
static const u8 u8Max = UINT8_MAX;
static const i8 i8Max = INT8_MAX;