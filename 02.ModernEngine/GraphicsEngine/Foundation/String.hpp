#pragma once
#include "Platform.hpp"


// Forward declarations //
struct Allocator;

template <typename K, typename V>
struct FlatHashMap;

struct FlatHashMapIterator;


// String view that references an already existing stream of chars.
struct StringView {
	char* text;
	sizet length;

	static bool Equals(const StringView& a, const StringView& b);
	static void CopyTo(const StringView& a, char* buffer, sizet bufferSize);
}; // struct StringView

//
// Class that preallocates a buffer and appends strings to it. Reserve an additional byte for the null termination when needed.
struct StringBuffer {

	void Init(sizet size, Allocator* allocator);
	void Shutdown();

	void Append(const char* string);
	void Append(const StringView& text);

	// Memory version of append.
	void AppendM(void* memory, sizet size);       
	void Append(const StringBuffer& otherBuffer);

	// Formatted version of append.
	void AppendF(const char* format, ...);        

	char* AppendUse(const char* string);
	char* AppendUseF(const char* format, ...);

	// Append and returns a pointer to the start. Used for strings mostly.
	char* AppendUse(const StringView& text);       
	
	// Append a substring of the passed string.
	char* AppendUseSubstring(const char* string, u32 startIndex, u32 end_index); 

	void CloseCurrentString();

	// Index interface
	u32 GetIndex(cstring text) const;
	cstring GetText(u32 index) const;

	char* Reserve(sizet size);

	char* Current() { return data + currentSize; }

	void Clear();

	char* data = nullptr;
	u32 bufferSize = 1024;
	u32 currentSize = 0;
	Allocator* allocator = nullptr;

}; // struct StringBuffer

//
//
struct StringArray {

	void Init(u32 size, Allocator* allocator);
	void Shutdown();
	void Clear();

	FlatHashMapIterator* BeginStringIteration();
	sizet GetStringCount() const;
	cstring GetString(u32 index) const;
	cstring GetNextString(FlatHashMapIterator* it) const;
	bool HasNextString(FlatHashMapIterator* it) const;

	cstring Intern(cstring string);

	FlatHashMap<u64, u32>* stringToIndex;    // Note: trying to avoid bringing the hash map header.
	FlatHashMapIterator* stringsIterator;

	char* data = nullptr;
	u32 bufferSize = 1024;
	u32 currentSize = 0;

	Allocator* allocator = nullptr;

}; // struct StringArray


