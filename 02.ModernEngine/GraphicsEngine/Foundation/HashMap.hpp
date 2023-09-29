#pragma once

#include "Memory.hpp"
#include "Assert.hpp"
#include "Bit.hpp"

#include "../external/wyhash.h"


// Hash Map /////////////////////////////////////////////////////////////////

static const u64 kIteratorEnd = u64Max;

//
//
struct FindInfo {
	u64 offset;
	u64 probeLength;
}; // struct FindInfo

//
//
struct FindResult {
	u64 index;
	bool freeIndex;  // States if the index is free or used.
}; // struct FindResult

//
// Iterator that stores the index of the entry.
struct FlatHashMapIterator {
	u64 index;

	bool isValid() const { return index != kIteratorEnd; }
	bool isInvalid() const { return index == kIteratorEnd; }
}; // struct FlatHashMapIterator

// A single block of empty control bytes for tables without any slots allocated.
// This enables removing a branch in the hot path of find().
i8* GroupInitEmpty();


// Probing ////////////////////////////////////////////////////////////
struct ProbeSequence {

	static const u64 kWidth = 16;   // TODO: this should be selectable.
	static const sizet kEngineHash = 0x31d3a36013e;

	ProbeSequence(u64 hash, u64 mask);

	u64 GetOffset() const;
	u64 GetOffset(u64 i) const;

	// 0-based probe index. The i-th probe in the probe sequence.
	u64 GetIndex() const;

	void Next();

	u64 mask;
	u64 offset;
	u64 index = 0;

}; // struct ProbeSequence

template <typename K, typename V>
struct FlatHashMap {

	struct KeyValue {
		K key;
		V value;
	}; // struct KeyValue

	void Init(Allocator* allocator, u64 initial_capacity);
	void Shutdown();

	// Main interface
	FlatHashMapIterator find(const K& key);
	void Insert(const K& key, const V& value);
	u32 Remove(const K& key);
	u32 Remove(const FlatHashMapIterator& it);

	V& Get(const K& key);
	V& Get(const FlatHashMapIterator& it);

	KeyValue& GetStructure(const K& key);
	KeyValue& GetStructure(const FlatHashMapIterator& it);

	void SetDefaultValue(const V& value);

	// Iterators
	FlatHashMapIterator IteratorBegin();
	void IteratorAdvance(FlatHashMapIterator& iterator);

	void Clear();
	void Reserve(u64 new_size);

	// Internal methods
	void EraseMeta(const FlatHashMapIterator& iterator);

	FindResult FindOrPrepareInsert(const K& key);
	FindInfo FindFirstNonFull(u64 hash);

	u64 PrepareInsert(u64 hash);

	ProbeSequence Probe(u64 hash);
	void RehashAndGrowIfNecessary();

	void DropDeletesWithoutResize();
	u64 CalculateSize(u64 new_capacity);

	void InitializeSlots();

	void Resize(u64 new_capacity);

	void IteratorSkipEmptyOrDeleted(FlatHashMapIterator& iterator);

	// Sets the control byte, and if `i < Group::kWidth - 1`, set the cloned byte
	// at the end too.
	void SetCtrl(u64 i, i8 h);
	void ResetCtrl();
	void resetGrowthLeft();


	i8* controlBytes = GroupInitEmpty();
	KeyValue* slots_ = nullptr;

	u64 size = 0;    // Occupied size
	u64 capacity = 0;    // Allocated capacity
	u64 growthLeft = 0;    // Number of empty space we can fill.

	Allocator* allocator = nullptr;
	KeyValue defaultKeyValue = { (K)-1, 0 };

}; // struct FlatHashMap

// Implementation /////////////////////////////////////////////////////
//
template<typename T>
inline u64 HashCalculate(const T& value, sizet seed = 0) {
	return wyhash(&value, sizeof(T), seed, _wyp);
}

template <size_t N>
inline u64 HashCalculate(const char(&value)[N], sizet seed = 0) {
	return wyhash(value, strlen(value), seed, _wyp);
}

template <>
inline u64 HashCalculate(const cstring& value, sizet seed) {
	return wyhash(value, strlen(value), seed, _wyp);
}

// Method to hash memory itself.
inline u64 HashBytes(void* data, sizet length, sizet seed = 0) {
	return wyhash(data, length, seed, _wyp);
}

// https://gankra.github.io/blah/hashbrown-tldr/
// https://blog.waffles.space/2018/12/07/deep-dive-into-hashbrown/
// https://abseil.io/blog/20180927-swisstables
//

// Control byte ///////////////////////////////////////////////////////
// Following Google's abseil library convetion - based on performance.
static const i8 k_control_bitmask_empty = -128; //0b10000000;
static const i8 k_control_bitmask_deleted = -2;   //0b11111110;
static const i8 k_control_bitmask_sentinel = -1;   //0b11111111;

static bool ControlIsEmpty(i8 control) { return control == k_control_bitmask_empty; }
static bool ControlIsFull(i8 control) { return control >= 0; }
static bool ControlIsDeleted(i8 control) { return control == k_control_bitmask_deleted; }
static bool ControlIsEmptyOrDeleted(i8 control) { return control < k_control_bitmask_sentinel; }

// Hashing ////////////////////////////////////////////////////////////

// Returns a hash seed.
//
// The seed consists of the ctrl_ pointer, which adds enough entropy to ensure
// non-determinism of iteration order in most cases.
// Implementation details: the low bits of the pointer have little or no entropy because of
// alignment. We shift the pointer to try to use higher entropy bits. A
// good number seems to be 12 bits, because that aligns with page size.
static u64 HashSeed(const i8* control) { return reinterpret_cast<uintptr_t>(control) >> 12; }

static u64 Hash1(u64 hash, const i8* ctrl) { return (hash >> 7) ^ HashSeed(ctrl); }
static i8 Hash2(u64 hash) { return hash & 0x7F; }


struct GroupSse2Impl {
	static constexpr size_t kWidth = 16;  // the number of slots per group

	explicit GroupSse2Impl(const i8* pos) {
		ctrl = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pos));
	}

	// Returns a bitmask representing the positions of slots that match hash.
	BitMask<uint32_t, kWidth> Match(i8 hash) const {
		auto match = _mm_set1_epi8(hash);
		return BitMask<uint32_t, kWidth>(
			_mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
	}

	// Returns a bitmask representing the positions of empty slots.
	BitMask<uint32_t, kWidth> MatchEmpty() const {
#if ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSSE3
		// This only works because kEmpty is -128.
		return BitMask<uint32_t, kWidth>(
			_mm_movemask_epi8(_mm_sign_epi8(ctrl, ctrl)));
#else
		return Match(static_cast<i8>(k_control_bitmask_empty));
#endif
	}

	// Returns a bitmask representing the positions of empty or deleted slots.
	BitMask<uint32_t, kWidth> MatchEmptyOrDeleted() const {
		auto special = _mm_set1_epi8(k_control_bitmask_sentinel);
		return BitMask<uint32_t, kWidth>(
			_mm_movemask_epi8(_mm_cmpgt_epi8(special, ctrl)));
	}

	// Returns the number of trailing empty or deleted elements in the group.
	uint32_t CountLeadingEmptyOrDeleted() const {
		auto special = _mm_set1_epi8(k_control_bitmask_sentinel);
		return TrailingZerosU32(static_cast<uint32_t>(
			_mm_movemask_epi8(_mm_cmpgt_epi8(special, ctrl)) + 1));
	}

	void ConvertSpecialToEmptyAndFullToDeleted(i8* dst) const {
		auto msbs = _mm_set1_epi8(static_cast<char>(-128));
		auto x126 = _mm_set1_epi8(126);
#if ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSSE3
		auto res = _mm_or_si128(_mm_shuffle_epi8(x126, ctrl), msbs);
#else
		auto zero = _mm_setzero_si128();
		auto special_mask = _mm_cmpgt_epi8(zero, ctrl);
		auto res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
#endif
		_mm_storeu_si128(reinterpret_cast<__m128i*>(dst), res);
	}

	__m128i ctrl;
};

// Capacity ///////////////////////////////////////////////////////////

//
static bool CapacityIsValid(size_t n);

// Rounds up the capacity to the Next power of 2 minus 1, with a minimum of 1.
static u64 CapacityNormalize(u64 n);

// General notes on capacity/growth methods below:
// - We use 7/8th as maximum load factor. For 16-wide groups, that gives an
//   average of two empty slots per group.
// - For (capacity+1) >= Group::kWidth, growth is 7/8*capacity.
// - For (capacity+1) < Group::kWidth, growth == capacity. In this case, we
//   never need to probe (the whole table fits in one group) so we don't need a
//   load factor less than 1.

// Given `capacity` of the table, returns the size (i.e. number of full slots)
// at which we should grow the capacity.
// if ( Group::kWidth == 8 && capacity == 7 ) { return 6 }
// x-x/8 does not work when x==7.
static u64  CapacityToGrowth(u64 capacity);
static u64  CapacityGrowthToLowerBound(u64 growth);


static void ConvertDeletedToEmptyAndFullToDeleted(i8* ctrl, size_t capacity) {
	//assert( ctrl[ capacity ] == k_control_bitmask_sentinel );
	//assert( IsValidCapacity( capacity ) );
	for (i8* pos = ctrl; pos != ctrl + capacity + 1; pos += GroupSse2Impl::kWidth) {
		GroupSse2Impl{ pos }.ConvertSpecialToEmptyAndFullToDeleted(pos);
	}
	// Copy the cloned ctrl bytes.
	MemoryCopy(ctrl + capacity + 1, ctrl, GroupSse2Impl::kWidth);
	ctrl[capacity] = k_control_bitmask_sentinel;
}


// FlatHashMap ////////////////////////////////////////////////////////
template <typename K, typename V>
void FlatHashMap<K, V>::ResetCtrl() {
	memset(controlBytes, k_control_bitmask_empty, capacity + GroupSse2Impl::kWidth);
	controlBytes[capacity] = k_control_bitmask_sentinel;
	//SanitizerPoisonMemoryRegion( slots_, sizeof( slot_type ) * capacity_ );
}

template <typename K, typename V>
void FlatHashMap<K, V>::resetGrowthLeft() {
	growthLeft = CapacityToGrowth(capacity) - size;
}

template <typename K, typename V>
ProbeSequence FlatHashMap<K, V>::Probe(u64 hash) {
	return ProbeSequence(Hash1(hash, controlBytes), capacity);
}

template<typename K, typename V>
inline void FlatHashMap<K, V>::Init(Allocator* allocator_, u64 initial_capacity) {
	allocator = allocator_;
	size = capacity = growthLeft = 0;
	defaultKeyValue = { (K)-1, (V)0 };

	controlBytes = GroupInitEmpty();
	slots_ = nullptr;
	Reserve(initial_capacity < 4 ? 4 : initial_capacity);
}

template<typename K, typename V>
inline void FlatHashMap<K, V>::Shutdown() {
	GFree(controlBytes, allocator);
}

template <typename K, typename V>
FlatHashMapIterator FlatHashMap<K, V>::find(const K& key) {

	const u64 hash = HashCalculate(key);
	ProbeSequence sequence = Probe(hash);

	while (true) {
		const GroupSse2Impl group{ controlBytes + sequence.GetOffset() };
		const i8 hash2 = Hash2(hash);
		for (int i : group.Match(hash2)) {
			const KeyValue& key_value = *(slots_ + sequence.GetOffset(i));
			if (key_value.key == key)
				return { sequence.GetOffset(i) };
		}

		if (group.MatchEmpty()) {
			break;
		}

		sequence.Next();
	}

	return { kIteratorEnd };
}

template <typename K, typename V>
void FlatHashMap<K, V>::Insert(const K& key, const V& value) {
	const FindResult find_result = FindOrPrepareInsert(key);
	if (find_result.freeIndex) {
		// Emplace
		slots_[find_result.index].key = key;
		slots_[find_result.index].value = value;
	}
	else {
		// Substitute value index
		slots_[find_result.index].value = value;
	}
}

template <typename K, typename V>
void FlatHashMap<K, V>::EraseMeta(const FlatHashMapIterator& iterator) {
	--size;

	const u64 index = iterator.index;
	const u64 index_before = (index - GroupSse2Impl::kWidth) & capacity;
	const auto empty_after = GroupSse2Impl(controlBytes + index).MatchEmpty();
	const auto empty_before = GroupSse2Impl(controlBytes + index_before).MatchEmpty();

	// We count how many consecutive non empties we have to the right and to the
	// left of `it`. If the sum is >= kWidth then there is at least one probe
	// window that might have seen a full group.
	const u64 trailing_zeros = empty_after.TrailingZeros();
	const u64 leading_zeros = empty_before.LeadingZeros();
	const u64 zeros = trailing_zeros + leading_zeros;
	//printf( "%x, %x", empty_after.TrailingZeros(), empty_before.LeadingZeros() );
	bool was_never_full = empty_before && empty_after;
	was_never_full = was_never_full && (zeros < GroupSse2Impl::kWidth);

	SetCtrl(index, was_never_full ? k_control_bitmask_empty : k_control_bitmask_deleted);
	growthLeft += was_never_full;
}

template <typename K, typename V>
u32 FlatHashMap<K, V>::Remove(const K& key) {
	FlatHashMapIterator iterator = find(key);
	if (iterator.index == kIteratorEnd)
		return 0;

	EraseMeta(iterator);
	return 1;
}

template<typename K, typename V>
inline u32 FlatHashMap<K, V>::Remove(const FlatHashMapIterator& iterator) {
	if (iterator.index == kIteratorEnd)
		return 0;

	EraseMeta(iterator);
	return 1;
}

template <typename K, typename V>
FindResult FlatHashMap<K, V>::FindOrPrepareInsert(const K& key) {
	u64 hash = HashCalculate(key);
	ProbeSequence sequence = Probe(hash);

	while (true) {
		const GroupSse2Impl group{ controlBytes + sequence.GetOffset() };
		for (int i : group.Match(Hash2(hash))) {
			const KeyValue& key_value = *(slots_ + sequence.GetOffset(i));
			if (key_value.key == key)
				return { sequence.GetOffset(i), false };
		}

		if (group.MatchEmpty()) {
			break;
		}

		sequence.Next();
	}
	return { PrepareInsert(hash), true };
}

template <typename K, typename V>
FindInfo FlatHashMap<K, V>::FindFirstNonFull(u64 hash) {
	ProbeSequence sequence = Probe(hash);

	while (true) {
		const GroupSse2Impl group{ controlBytes + sequence.GetOffset() };
		auto mask = group.MatchEmptyOrDeleted();

		if (mask) {
			return { sequence.GetOffset(mask.LowestBitSet()), sequence.GetIndex() };
		}

		sequence.Next();
	}

	return FindInfo();
}

template <typename K, typename V>
u64 FlatHashMap<K, V>::PrepareInsert(u64 hash) {
	FindInfo find_info = FindFirstNonFull(hash);
	if (growthLeft == 0 && !ControlIsDeleted(controlBytes[find_info.offset])) {
		RehashAndGrowIfNecessary();
		find_info = FindFirstNonFull(hash);
	}
	++size;

	growthLeft -= ControlIsEmpty(controlBytes[find_info.offset]) ? 1 : 0;
	SetCtrl(find_info.offset, Hash2(hash));
	return find_info.offset;
}

template <typename K, typename V>
void FlatHashMap<K, V>::RehashAndGrowIfNecessary() {
	if (capacity == 0) {
		Resize(1);
	}
	else if (size <= CapacityToGrowth(capacity) / 2) {
		// Squash DELETED without growing if there is enough capacity.
		DropDeletesWithoutResize();
	}
	else {
		// Otherwise grow the container.
		Resize(capacity * 2 + 1);
	}
}

template <typename K, typename V>
void FlatHashMap<K, V>::DropDeletesWithoutResize() {
	//assert( IsValidCapacity( capacity_ ) );
	//assert( !is_small( capacity_ ) );
	// Algorithm:
	// - mark all DELETED slots as EMPTY
	// - mark all FULL slots as DELETED
	// - for each slot marked as DELETED
	// hash = Hash(element)
	// target = find_first_non_full(hash)
	// if target is in the same group
	//  mark slot as FULL
	// else if target is EMPTY
	//  transfer element to target
	//  mark slot as EMPTY
	//  mark target as FULL
	// else if target is DELETED
	//  swap current element with target element
	//  mark target as FULL
	//  repeat procedure for current slot with moved from element (target)
	//ConvertDeletedToEmptyAndFullToDeleted( controlBytes, capacity );

	alignas(KeyValue) unsigned char raw[sizeof(KeyValue)];
	size_t totalProbeLength = 0;
	KeyValue* slot = reinterpret_cast<KeyValue*>(&raw);
	for (size_t i = 0; i != capacity; ++i) {
		if (!ControlIsDeleted(controlBytes[i])) {
			continue;
		}

		const KeyValue* current_slot = slots_ + i;
		size_t hash = HashCalculate(current_slot->key);
		auto target = FindFirstNonFull(hash);
		size_t newI = target.offset;
		totalProbeLength += target.probeLength;

		// Verify if the old and new i fall within the same group wrt the hash.
		// If they do, we don't need to move the object as it falls already in the
		// best probe we can.
		const auto probeIndex = [&](size_t pos) {
			return ((pos - Probe(hash).GetOffset()) & capacity) / GroupSse2Impl::kWidth;
			};

		// Element doesn't move.
		if ((probeIndex(newI) == probeIndex(i))) {
			SetCtrl(i, Hash2(hash));
			continue;
		}
		if (ControlIsEmpty(controlBytes[newI])) {
			// Transfer element to the empty spot.
			// SetCtrl poisons/unpoisons the slots so we have to call it at the
			// right time.
			SetCtrl(newI, Hash2(hash));
			memcpy(slots_ + newI, slots_ + i, sizeof(KeyValue));
			SetCtrl(i, k_control_bitmask_empty);
		}
		else {
			//assert( ControlIsDeleted( controlBytes[ newI ] ) );
			SetCtrl(newI, Hash2(hash));
			// Until we are done rehashing, DELETED marks previously FULL slots.
			// Swap i and newI elements.
			memcpy(slot, slots_ + i, sizeof(KeyValue));
			memcpy(slots_ + i, slots_ + newI, sizeof(KeyValue));
			memcpy(slots_ + newI, slot, sizeof(KeyValue));
			--i;  // repeat
		}
	}

	resetGrowthLeft();
}

template <typename K, typename V>
u64 FlatHashMap<K, V>::CalculateSize(u64 new_capacity) {
	return (new_capacity + GroupSse2Impl::kWidth + new_capacity * (sizeof(KeyValue)));
}

template <typename K, typename V>
void FlatHashMap<K, V>::InitializeSlots() {

	char* new_memory = (char*)GAlloca(CalculateSize(capacity), allocator);

	controlBytes = reinterpret_cast<i8*>(new_memory);
	slots_ = reinterpret_cast<KeyValue*>(new_memory + capacity + GroupSse2Impl::kWidth);

	ResetCtrl();
	resetGrowthLeft();
}

template <typename K, typename V>
void FlatHashMap<K, V>::Resize(u64 new_capacity) {
	//assert( IsValidCapacity( new_capacity ) );
	i8* oldControlBytes = controlBytes;
	KeyValue* oldSlots = slots_;
	const u64 oldCapacity = capacity;

	capacity = new_capacity;

	InitializeSlots();

	size_t totalProbeLength = 0;
	for (size_t i = 0; i != oldCapacity; ++i) {
		if (ControlIsFull(oldControlBytes[i])) {
			const KeyValue* oldValue = oldSlots + i;
			u64 hash = HashCalculate(oldValue->key);

			FindInfo findInfo = FindFirstNonFull(hash);

			u64 newI = findInfo.offset;
			totalProbeLength += findInfo.probeLength;

			SetCtrl(newI, Hash2(hash));

			MemoryCopy(slots_ + newI, oldSlots + i, sizeof(KeyValue));
		}
	}

	if (oldCapacity) {
		GFree(oldControlBytes, allocator);
	}
}

// Sets the control byte, and if `i < Group::kWidth - 1`, set the cloned byte
// at the end too.
template <typename K, typename V>
void FlatHashMap<K, V>::SetCtrl(u64 i, i8 h) {
	/*assert( i < capacity_ );

	if ( IsFull( h ) ) {
		SanitizerUnpoisonObject( slots_ + i );
	} else {
		SanitizerPoisonObject( slots_ + i );
	}*/

	controlBytes[i] = h;
	constexpr size_t kClonedBytes = GroupSse2Impl::kWidth - 1;
	controlBytes[((i - kClonedBytes) & capacity) + (kClonedBytes & capacity)] = h;
}

template <typename K, typename V>
V& FlatHashMap<K, V>::Get(const K& key) {
	FlatHashMapIterator iterator = find(key);
	if (iterator.index != kIteratorEnd)
		return slots_[iterator.index].value;
	return defaultKeyValue.value;
}

template<typename K, typename V>
V& FlatHashMap<K, V>::Get(const FlatHashMapIterator& iterator) {
	if (iterator.index != kIteratorEnd)
		return slots_[iterator.index].value;
	return defaultKeyValue.value;
}

template <typename K, typename V>
typename FlatHashMap<K, V>::KeyValue& FlatHashMap<K, V>::GetStructure(const K& key) {
	FlatHashMapIterator iterator = find(key);
	if (iterator.index != kIteratorEnd)
		return slots_[iterator.index];
	return defaultKeyValue;
}

template<typename K, typename V>
typename FlatHashMap<K, V>::KeyValue& FlatHashMap<K, V>::GetStructure(const FlatHashMapIterator& iterator) {
	return slots_[iterator.index];
}

template<typename K, typename V>
inline void FlatHashMap<K, V>::SetDefaultValue(const V& value) {
	defaultKeyValue.value = value;
}

template<typename K, typename V>
FlatHashMapIterator FlatHashMap<K, V>::IteratorBegin() {
	FlatHashMapIterator it{ 0 };

	IteratorSkipEmptyOrDeleted(it);

	return it;
}

template<typename K, typename V>
void FlatHashMap<K, V>::IteratorAdvance(FlatHashMapIterator& iterator) {

	iterator.index++;

	IteratorSkipEmptyOrDeleted(iterator);
}

template<typename K, typename V>
inline void FlatHashMap<K, V>::IteratorSkipEmptyOrDeleted(FlatHashMapIterator& it) {
	i8* ctrl = controlBytes + it.index;

	while (ControlIsEmptyOrDeleted(*ctrl)) {
		u32 shift = GroupSse2Impl{ ctrl }.CountLeadingEmptyOrDeleted();
		ctrl += shift;
		it.index += shift;
	}
	if (*ctrl == k_control_bitmask_sentinel)
		it.index = kIteratorEnd;
}

template<typename K, typename V>
inline void FlatHashMap<K, V>::Clear() {
	size = 0;
	ResetCtrl();
	resetGrowthLeft();
}

template<typename K, typename V>
inline void FlatHashMap<K, V>::Reserve(u64 new_size) {
	if (new_size > size + growthLeft) {
		size_t m = CapacityGrowthToLowerBound(new_size);
		Resize(CapacityNormalize(m));
	}
}

// Capacity ///////////////////////////////////////////////////////////
bool CapacityIsValid(size_t n) { return ((n + 1) & n) == 0 && n > 0; }

inline u64 lzcnt_soft(u64 n) {
	// NOTE(marco): the __lzcnt intrisics require at least haswell
#if defined(_MSC_VER)
	unsigned long index = 0;
	_BitScanReverse64(&index, n);
	u64 cnt = index ^ 63;
#else
	u64 cnt = __builtin_clzl(n);
#endif
	return cnt;
}

// Rounds up the capacity to the Next power of 2 minus 1, with a minimum of 1.
u64 CapacityNormalize(u64 n) { return n ? ~u64{} >> lzcnt_soft(n) : 1; }

//
u64 CapacityToGrowth(u64 capacity) { return capacity - capacity / 8; }

//
u64 CapacityGrowthToLowerBound(u64 growth) { return growth + static_cast<u64>((static_cast<i64>(growth) - 1) / 7); }


// Grouping: implementation ///////////////////////////////////////////
inline i8* GroupInitEmpty() {
	alignas(16) static constexpr i8 empty_group[] = {
		k_control_bitmask_sentinel, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty,
		k_control_bitmask_empty,    k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty, k_control_bitmask_empty };
	return const_cast<i8*>(empty_group);
}


// Probing: implementation ////////////////////////////////////////////
inline ProbeSequence::ProbeSequence(u64 hash_, u64 mask_) {
	//assert( ( ( mask_ + 1 ) & mask_ ) == 0 && "not a mask" );
	mask = mask_;
	offset = hash_ & mask_;
}

inline u64 ProbeSequence::GetOffset() const {
	return offset;
}

inline u64 ProbeSequence::GetOffset(u64 i) const {
	return (offset + i) & mask;
}

inline u64 ProbeSequence::GetIndex() const {
	return index;
}

inline void ProbeSequence::Next() {
	index += kWidth;
	offset += index;
	offset &= mask;
}

