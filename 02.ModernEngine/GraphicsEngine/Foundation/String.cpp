#include "String.hpp"
#include "Memory.hpp"
#include "Log.hpp"
#include "Assert.hpp"

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>

#include "HashMap.hpp"

#define ASSERT_ON_OVERFLOW

#if defined (ASSERT_ON_OVERFLOW)
#define GASSERT_OVERFLOW() GASSERT(false)
#else
#define GASSERT_OVERFLOW()
#endif // ASSERT_ON_OVERFLOW


// StringView ///////////////////////////////////////////////////////////////////
bool StringView::Equals(const StringView& a, const StringView& b) {
    if (a.length != b.length)
        return false;

    for (u32 i = 0; i < a.length; ++i) {
        if (a.text[i] != b.text[i]) {
            return false;
        }
    }

    return true;
}

void StringView::CopyTo(const StringView& a, char* buffer, sizet bufferSize) {
    // Take in account the null vector
    const sizet max_length = bufferSize - 1 < a.length ? bufferSize - 1 : a.length;
    MemoryCopy(buffer, a.text, max_length);
    buffer[a.length] = 0;
}


//
// StringBuffer /////////////////////////////////////////////////////////////////
void StringBuffer::Init(sizet size, Allocator* allocator_) {
    if (data) {
        allocator->Deallocate(data);
    }

    if (size < 1) {
        GPrint("ERROR: Buffer cannot be empty!\n");
        return;
    }
    allocator = allocator_;
    data = (char*)GAlloca(size + 1, allocator_);
    GASSERT(data);
    data[0] = 0;
    bufferSize = (u32)size;
    currentSize = 0;
}

void StringBuffer::Shutdown() {

    GFree(data, allocator);

    bufferSize = currentSize = 0;
}

void StringBuffer::Append(const char* string) {
    AppendF("%s", string);
}

void StringBuffer::AppendF(const char* format, ...) {
    if (currentSize >= bufferSize) {
        GASSERT_OVERFLOW();
        GPrint("Buffer full! Please allocate more size.\n");
        return;
    }

    // TODO: safer version!
    va_list args;
    va_start(args, format);
#if defined(_MSC_VER)
    int writtenChars = vsnprintf_s(&data[currentSize], bufferSize - currentSize, _TRUNCATE, format, args);
#else
    int writtenChars = vsnprintf(&data[currentSize], bufferSize - currentSize, format, args);
#endif

    currentSize += writtenChars > 0 ? writtenChars : 0;
    va_end(args);

    if (writtenChars < 0) {
        GASSERT_OVERFLOW();
        GPrint("New string too big for current buffer! Please allocate more size.\n");
    }
}

void StringBuffer::Append(const StringView& text) {
    const sizet max_length = currentSize + text.length < bufferSize ? text.length : bufferSize - currentSize;
    if (max_length == 0 || max_length >= bufferSize) {
        GASSERT_OVERFLOW();
        GPrint("Buffer full! Please allocate more size.\n");
        return;
    }

    memcpy(&data[currentSize], text.text, max_length);
    currentSize += (u32)max_length;

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[currentSize] = 0;
}

void StringBuffer::AppendM(void* memory, sizet size) {

    if (currentSize + size >= bufferSize) {
        GASSERT_OVERFLOW();
        GPrint("Buffer full! Please allocate more size.\n");
        return;
    }

    memcpy(&data[currentSize], memory, size);
    currentSize += (u32)size;
}

void StringBuffer::Append(const StringBuffer& other_buffer) {

    if (other_buffer.currentSize == 0) {
        return;
    }

    if (currentSize + other_buffer.currentSize >= bufferSize) {
        GASSERT_OVERFLOW();
        GPrint("Buffer full! Please allocate more size.\n");
        return;
    }

    memcpy(&data[currentSize], other_buffer.data, other_buffer.currentSize);
    currentSize += other_buffer.currentSize;
}


char* StringBuffer::AppendUse(const char* string) {
    return AppendUseF("%s", string);
}

char* StringBuffer::AppendUseF(const char* format, ...) {
    u32 cached_offset = this->currentSize;

    // TODO: safer version!
    // TODO: do not copy paste!
    if (currentSize >= bufferSize) {
        GASSERT_OVERFLOW();
        GPrint("Buffer full! Please allocate more size.\n");
        return nullptr;
    }

    va_list args;
    va_start(args, format);
#if defined(_MSC_VER)
    int writtenChars = vsnprintf_s(&data[currentSize], bufferSize - currentSize, _TRUNCATE, format, args);
#else
    int writtenChars = vsnprintf(&data[currentSize], bufferSize - currentSize, format, args);
#endif
    currentSize += writtenChars > 0 ? writtenChars : 0;
    va_end(args);

    if (writtenChars < 0) {
        GPrint("New string too big for current buffer! Please allocate more size.\n");
    }

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[currentSize] = 0;
    ++currentSize;

    return this->data + cached_offset;
}

char* StringBuffer::AppendUse(const StringView& text) {
    u32 cached_offset = this->currentSize;

    Append(text);
    ++currentSize;

    return this->data + cached_offset;
}

char* StringBuffer::AppendUseSubstring(const char* string, u32 start_index, u32 end_index) {
    u32 size = end_index - start_index;
    if (currentSize + size >= bufferSize) {
        GASSERT_OVERFLOW();
        GPrint("Buffer full! Please allocate more size.\n");
        return nullptr;
    }

    u32 cached_offset = this->currentSize;

    memcpy(&data[currentSize], string, size);
    currentSize += size;

    data[currentSize] = 0;
    ++currentSize;

    return this->data + cached_offset;
}

void StringBuffer::CloseCurrentString() {
    data[currentSize] = 0;
    ++currentSize;
}

u32 StringBuffer::GetIndex(cstring text) const {
    u64 textDistance = text - data;
    // TODO: how to handle an error here ?
    return textDistance < bufferSize ? u32(textDistance) : u32Max;
}

cstring StringBuffer::GetText(u32 index) const {
    // TODO: how to handle an error here ?
    return index < bufferSize ? cstring(data + index) : nullptr;
}

char* StringBuffer::Reserve(sizet size) {
    if (currentSize + size >= bufferSize)
        return nullptr;

    u32 offset = currentSize;
    currentSize += (u32)size;

    return data + offset;
}

void StringBuffer::Clear() {
    currentSize = 0;
    data[0] = 0;
}

// StringArray ////////////////////////////////////////////////////////////
void StringArray::Init(u32 size, Allocator* allocator_) {

    allocator = allocator_;
    // Allocate also memory for the hash map
    char* allocated_memory = (char*)allocator_->Allocate(size + sizeof(FlatHashMap<u64, u32>) + sizeof(FlatHashMapIterator), 1);
    stringToIndex = (FlatHashMap<u64, u32>*)allocated_memory;
    stringToIndex->Init(allocator, 8);
    stringToIndex->set_default_value(u32Max);

    stringsIterator = (FlatHashMapIterator*)(allocated_memory + sizeof(FlatHashMap<u64, u32>));

    data = allocated_memory + sizeof(FlatHashMap<u64, u32>) + sizeof(FlatHashMapIterator);

    bufferSize = size;
    currentSize = 0;
}

void StringArray::Shutdown() {
    // stringToIndex contains ALL the memory including data.
    GFree(stringToIndex, allocator);

    bufferSize = currentSize = 0;
}

void StringArray::Clear() {
    currentSize = 0;

    stringToIndex->Clear();
}

FlatHashMapIterator* StringArray::BeginStringIteration() {
    *stringsIterator = stringToIndex->iterator_begin();
    return stringsIterator;
}

sizet StringArray::GetStringCount() const {
    return stringToIndex->size;
}

cstring StringArray::GetNextString(FlatHashMapIterator* it) const {
    u32 index = stringToIndex->get(*it);
    stringToIndex->iterator_advance(*it);
    cstring string = GetString(index);
    return string;
}

bool StringArray::HasNextString(FlatHashMapIterator* it) const {
    return it->is_valid();
}

cstring StringArray::GetString(u32 index) const {
    u32 dataIndex = index;
    if (dataIndex < currentSize) {
        return data + dataIndex;
    }
    return nullptr;
}

cstring StringArray::Intern(cstring string) {
    static sizet seed = 0xf2ea4ffad;
    const sizet length = strlen(string);
    const sizet hashed_string = hash_bytes((void*)string, length, seed);

    u32 stringIndex = stringToIndex->get(hashed_string);
    if (stringIndex != u32Max) {
        return data + stringIndex;
    }

    stringIndex = currentSize;
    // Increase current buffer with new interned string
    currentSize += (u32)length + 1; // null termination
    //strcpy(data + stringIndex, string);
    strcpy_s(data + stringIndex, length + 1, string); /// TODO Test!

    // Update hash map
    stringToIndex->insert(hashed_string, stringIndex);

    return data + stringIndex;
}

