#pragma once

#include "Platform.hpp"

struct Allocator;

// Common methods //

/// <summary>
/// Count non-significant zeroes in a u32.
/// </summary>
/// <param name="x">u32 number</param>
/// <returns>Number of non-significant zeroes</returns>
u32 LeadingZeroesU32(u32 x);

#if defined(_MSC_VER)
/// <summary>
/// Count non-significant zeroes in a u32. For MSVC compiler.
/// </summary>
/// <param name="x">u32 number</param>
/// <returns>Number of non-significant zeroes</returns>
u32 LeadingZeroesU32Msvc(u32 x);
#endif

/// <summary>
/// Count trailing zeroes in a u32. When source operand is 0, it returns its size in bits.
/// </summary>
/// <param name="x">u32 number</param>
/// <returns>Number of trailing zeroes</returns>
u32 TrailingZerosU32(u32 x);

/// <summary>
/// Count trailing zeroes in a u64. When source operand is 0, it returns its size in bits.
/// </summary>
/// <param name="x">u64 number</param>
/// <returns>Number of trailing zeroes</returns>
u64 TrailingZerosU64(u64 x);

/// <summary>
/// Round a u32 to power of two.
/// </summary>
/// <param name="v">u32 number</param>
/// <returns>Rounded power of two</returns>
u32 RoundUpToPowerOf2(u32 v);

/// <summary>
/// Print a u64 number to binary
/// </summary>
/// <param name="n">u64 number</param>
void PrintBinary(u64 n);

/// <summary>
/// Print a u32 number to binary
/// </summary>
/// <param name="n">u32 number</param>
void PrintBinary(u32 n);


/// <summary>
/// An abstraction over a bitmask. It provides an easy way to iterate through the
/// indexes of the set bits of a bitmask.  When Shift=0 (platforms with SSE),
/// this is a true bitmask.  On non-SSE, platforms the arithematic used to
/// emulate the SSE behavior works in bytes (Shift=3) and leaves each bytes as
/// either 0x00 or 0x80.
/// For example:
///   for (int i : BitMask[uint32_t, 16] (0x5)) => yields 0, 2
///   for (int i : BitMask[uint64_t, 8, 3] (0x0000000080800000)) -> yields 2, 3
/// (Here squared brackets represent stripes, but it is incorrect character in XML.)
/// </summary>
/// <typeparam name="T"></typeparam>
/// <typeparam name="SignificantBits"></typeparam>
/// <typeparam name="Shift"></typeparam>
template <class T, int SignificantBits, int Shift = 0>
class BitMask {
	//static_assert( std::is_unsigned<T>::value, "" );
	//static_assert( Shift == 0 || Shift == 3, "" );

public:
	// These are useful for unit tests (gunit).
	using value_type = int;
	using iterator = BitMask;
	using const_iterator = BitMask;

	explicit BitMask(T maskP) : mask(maskP) {
	}

	BitMask& operator++() {
		mask &= (mask - 1);
		return *this;
	}

	explicit operator bool() const {
		return mask != 0;
	}

	int operator*() const {
		return LowestBitSet();
	}

	uint32_t LowestBitSet() const {
		return TrailingZerosU32(mask) >> Shift;
	}

	uint32_t HighestBitSet() const {
		return static_cast<uint32_t>((bit_width(mask) - 1) >> Shift);
	}

	BitMask begin() const {
		return *this;
	}

	BitMask end() const {
		return BitMask(0);
	}

	uint32_t TrailingZeros() const {
		return TrailingZerosU32(mask);// >> Shift;
	}

	uint32_t LeadingZeros() const {
		return LeadingZeroesU32(mask);// >> Shift;
	}

private:
	friend bool operator==(const BitMask& a, const BitMask& b) {
		return a.mask == b.mask;
	}
	friend bool operator!=(const BitMask& a, const BitMask& b) {
		return a.mask != b.mask;
	}

	T mask;
}; // class BitMask

// Utility methods

/// <summary>
/// Returns a number modulo the first 8 bits
/// </summary>
/// <param name="bit">Number</param>
/// <returns>A number on 8 bits</returns>
inline u32 BitMask8(u32 bit) { return 1 << (bit & 7); }

inline u32 BitSlot8(u32 bit) { return bit / 8; }

//
//
struct BitSet {

	void Init(Allocator* allocator, u32 totalBits);
	void Shutdown();

	void Resize(u32 totalBits);

	void SetBit(u32 index) {
		bits[index / 8] |= BitMask8(index);
	}

	void ClearBit(u32 index) {
		bits[index / 8] &= ~BitMask8(index);
	}

	u8 GetBit(u32 index) {
		return bits[index / 8] & BitMask8(index);
	}

	Allocator* allocator = nullptr;
	u8* bits = nullptr;
	u32 size = 0;

}; // struct BitSet

//
//
template <u32 SizeInBytes>
struct BitSetFixed {

	void SetBit(u32 index) { bits[index / 8] |= BitMask8(index); }
	void ClearBit(u32 index) { bits[index / 8] &= ~BitMask8(index); }
	u8 GetBit(u32 index) { return bits[index / 8] & BitMask8(index); }

	u8 bits[SizeInBytes];

}; // struct BitSetFixed


