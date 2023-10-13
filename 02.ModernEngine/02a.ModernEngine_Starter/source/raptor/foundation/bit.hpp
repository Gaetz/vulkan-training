#pragma once

#include "foundation/platform.hpp"

namespace raptor {

	struct Allocator;

	// Common methods /////////////////////////////////////////////////////


	/// <summary>
	/// Count non-significant zeroes in a u32.
	/// </summary>
	/// <param name="x">u32 number</param>
	/// <returns>Number of non-significant zeroes</returns>
	u32             leading_zeroes_u32(u32 x);
#if defined(_MSC_VER)

	/// <summary>
	/// Count non-significant zeroes in a u32. For MSVC compiler.
	/// </summary>
	/// <param name="x">u32 number</param>
	/// <returns>Number of non-significant zeroes</returns>
	u32             leading_zeroes_u32_msvc(u32 x);
#endif

	/// <summary>
	/// Count trailing zeroes in a u32. When source operand is 0, it returns its size in bits.
	/// </summary>
	/// <param name="x">u32 number</param>
	/// <returns>Number of trailing zeroes</returns>
	u32             trailing_zeros_u32(u32 x);

	/// <summary>
	/// Count trailing zeroes in a u64. When source operand is 0, it returns its size in bits.
	/// </summary>
	/// <param name="x">u64 number</param>
	/// <returns>Number of trailing zeroes</returns>
	u64             trailing_zeros_u64(u64 x);

	/// <summary>
	/// Round a u32 to power of two.
	/// </summary>
	/// <param name="v">u32 number</param>
	/// <returns>Rounded power of two</returns>
	u32             round_up_to_power_of_2(u32 v);

	/// <summary>
	/// Print a u64 number to binary
	/// </summary>
	/// <param name="n">u64 number</param>
	void            print_binary(u64 n);

	/// <summary>
	/// Print a u32 number to binary
	/// </summary>
	/// <param name="n">u32 number</param>
	void            print_binary(u32 n);

	// class BitMask //////////////////////////////////////////////////////

	// An abstraction over a bitmask. It provides an easy way to iterate through the
	// indexes of the set bits of a bitmask.  When Shift=0 (platforms with SSE),
	// this is a true bitmask.  On non-SSE, platforms the arithematic used to
	// emulate the SSE behavior works in bytes (Shift=3) and leaves each bytes as
	// either 0x00 or 0x80.
	//
	// For example:
	//   for (int i : BitMask<uint32_t, 16>(0x5)) -> yields 0, 2
	//   for (int i : BitMask<uint64_t, 8, 3>(0x0000000080800000)) -> yields 2, 3
	template <class T, int SignificantBits, int Shift = 0>
	class BitMask {
		//static_assert( std::is_unsigned<T>::value, "" );
		//static_assert( Shift == 0 || Shift == 3, "" );

	public:
		// These are useful for unit tests (gunit).
		using value_type = int;
		using iterator = BitMask;
		using const_iterator = BitMask;

		explicit BitMask(T mask) : mask_(mask) {
		}
		BitMask& operator++() {
			mask_ &= (mask_ - 1);
			return *this;
		}
		explicit operator bool() const {
			return mask_ != 0;
		}
		int operator*() const {
			return LowestBitSet();
		}
		uint32_t LowestBitSet() const {
			return trailing_zeros_u32(mask_) >> Shift;
		}
		uint32_t HighestBitSet() const {
			return static_cast<uint32_t>((bit_width(mask_) - 1) >> Shift);
		}

		BitMask begin() const {
			return *this;
		}
		BitMask end() const {
			return BitMask(0);
		}

		uint32_t TrailingZeros() const {
			return trailing_zeros_u32(mask_);// >> Shift;
		}

		uint32_t LeadingZeros() const {
			return leading_zeroes_u32(mask_);// >> Shift;
		}

	private:
		friend bool operator==(const BitMask& a, const BitMask& b) {
			return a.mask_ == b.mask_;
		}
		friend bool operator!=(const BitMask& a, const BitMask& b) {
			return a.mask_ != b.mask_;
		}

		T mask_;
	}; // class BitMask

	// Utility methods
	inline u32              bit_mask_8(u32 bit) { return 1 << (bit & 7); }
	inline u32              bit_slot_8(u32 bit) { return bit / 8; }

	//
	//
	struct BitSet {

		void                init(Allocator* allocator, u32 total_bits);
		void                shutdown();

		void                resize(u32 total_bits);

		void                set_bit(u32 index) { bits[index / 8] |= bit_mask_8(index); }
		void                clear_bit(u32 index) { bits[index / 8] &= ~bit_mask_8(index); }
		u8                  get_bit(u32 index) { return bits[index / 8] & bit_mask_8(index); }

		Allocator* allocator = nullptr;
		u8* bits = nullptr;
		u32                 size = 0;

	}; // struct BitSet

	//
	//
	template <u32 SizeInBytes>
	struct BitSetFixed {

		void                set_bit(u32 index) { bits[index / 8] |= bit_mask_8(index); }
		void                clear_bit(u32 index) { bits[index / 8] &= ~bit_mask_8(index); }
		u8                  get_bit(u32 index) { return bits[index / 8] & bit_mask_8(index); }

		u8                  bits[SizeInBytes];

	}; // struct BitSetFixed

} // namespace raptor
