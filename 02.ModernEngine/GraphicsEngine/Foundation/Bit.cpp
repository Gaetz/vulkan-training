#include "Bit.hpp"
#include "Log.hpp"
#include "Memory.hpp"

#if defined(_MSC_VER)
#include <immintrin.h>
#include <intrin0.h>
#endif
#include <string.h>

u32 TrailingZerosU32(u32 x) {
    /*unsigned long result = 0;  // NOLINT(runtime/int)
    _BitScanForward( &result, x );
    return result;*/
#if defined(_MSC_VER)
    return _tzcnt_u32(x);
#else
    return __builtin_ctz(x);
#endif
}

u32 LeadingZeroesU32(u32 x) {
    /*unsigned long result = 0;  // NOLINT(runtime/int)
    _BitScanReverse( &result, x );
    return result;*/
#if defined(_MSC_VER)
    return __lzcnt(x);
#else
    return __builtin_clz(x);
#endif
}

#if defined(_MSC_VER)
u32 LeadingZeroesU32Msvc(u32 x) {
    unsigned long result = 0;  // NOLINT(runtime/int)
    if (_BitScanReverse(&result, x)) {
        return 31 - result;
    }
    return 32;
}
#endif

u64 TrailingZerosU64(u64 x) {
#if defined(_MSC_VER)
    return _tzcnt_u64(x);
#else
    return __builtin_ctzl(x);
#endif
}

u32 RoundUpToPowerOf2(u32 v) {

    u32 nv = 1 << (32 - LeadingZeroesU32(v));
    return nv;
}
void PrintBinary(u64 n) {

    GPrint("0b");
    for (u32 i = 0; i < 64; ++i) {
        u64 bit = (n >> (64 - i - 1)) & 0x1;
        GPrint("%llu", bit);
    }
    GPrint(" ");
}

void PrintBinary(u32 n) {

    GPrint("0b");
    for (u32 i = 0; i < 32; ++i) {
        u32 bit = (n >> (32 - i - 1)) & 0x1;
        GPrint("%u", bit);
    }
    GPrint(" ");
}

// BitSet //
void BitSet::Init(Allocator* allocator_, u32 total_bits) {
    allocator = allocator_;
    bits = nullptr;
    size = 0;

    Resize(total_bits);
}

void BitSet::Shutdown() {
    GFree(bits, allocator);
}

void BitSet::Resize(u32 total_bits) {
    u8* old_bits = bits;

    const u32 new_size = (total_bits + 7) / 8;
    if (size == new_size) {
        return;
    }

    bits = (u8*)GAllocaM(new_size, allocator);

    if (old_bits) {
        memcpy(bits, old_bits, size);
        GFree(old_bits, allocator);
    }
    else {
        memset(bits, 0, new_size);
    }

    size = new_size;
}
