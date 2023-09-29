#define RAPTOR_BLOB_WRITE
#include <string.h>
#include "BlobSerialization.hpp"

#include <stdio.h>
#include <stdarg.h>

void BlobSerializer::WriteCommon(Allocator* allocator_, u32 serializerVersion_, sizet size) {
    allocator = allocator_;
    // Allocate memory
    blobMemory = (char*)GAlloca(size + sizeof(BlobHeader), allocator_);
    GASSERT(blobMemory);

    hasAllocatedMemory = 1;

    totalSize = (u32)size + sizeof(BlobHeader);
    serializedOffset = allocatedOffset = 0;

    serializerVersion = serializerVersion_;
    // This will be written into the blob
    dataVersion = serializerVersion_;
    isReading = 0;
    isMappable = 0;

    // Write header
    BlobHeader* header = (BlobHeader*)AllocateStatic(sizeof(BlobHeader));
    header->version = serializerVersion;
    header->mappable = isMappable;

    serializedOffset = allocatedOffset;
}

void BlobSerializer::Shutdown() {

    if (isReading) {
        // When reading and serializing, we can free blob memory after read.
        // Otherwise we will free the pointer when done.
        if (blobMemory && hasAllocatedMemory)
            GFree(blobMemory, allocator);
    }
    else {
        if (blobMemory)
            GFree(blobMemory, allocator);
    }

    /*if ( blobMemory )
        GFree( blobMemory, allocator );*/


    serializedOffset = allocatedOffset = 0;
}

void BlobSerializer::Serialize(char* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(char));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(char));
    }

    serializedOffset += sizeof(char);
}

void BlobSerializer::Serialize(i8* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(i8));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(i8));
    }

    serializedOffset += sizeof(i8);
}

void BlobSerializer::Serialize(u8* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(u8));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(u8));
    }

    serializedOffset += sizeof(u8);
}

void BlobSerializer::Serialize(i16* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(i16));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(i16));
    }

    serializedOffset += sizeof(i16);
}

void BlobSerializer::Serialize(u16* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(u16));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(u16));
    }

    serializedOffset += sizeof(u16);
}

void BlobSerializer::Serialize(i32* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(i32));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(i32));
    }

    serializedOffset += sizeof(i32);
}

void BlobSerializer::Serialize(u32* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(u32));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(u32));
    }

    serializedOffset += sizeof(u32);
}

void BlobSerializer::Serialize(i64* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(i64));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(i64));
    }

    serializedOffset += sizeof(i64);
}

void BlobSerializer::Serialize(u64* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(u64));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(u64));
    }

    serializedOffset += sizeof(u64);
}

void BlobSerializer::Serialize(f32* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(f32));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(f32));
    }

    serializedOffset += sizeof(f32);
}

void BlobSerializer::Serialize(f64* data) {

    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(f64));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(f64));
    }

    serializedOffset += sizeof(f64);
}

void BlobSerializer::Serialize(bool* data) {
    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], sizeof(bool));
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, sizeof(bool));
    }

    serializedOffset += sizeof(bool);
}

void BlobSerializer::SerializeMemory(void* data, sizet size) {

    if (isReading) {
        memcpy(data, &blobMemory[serializedOffset], size);
    }
    else {
        memcpy(&blobMemory[serializedOffset], data, size);
    }

    serializedOffset += (u32)size;
}

void BlobSerializer::SerializeMemoryBlock(void** data, u32* size) {

    Serialize(size);

    if (isReading) {
        // Blob --> Data
        i32 sourceDataOffset;
        Serialize(&sourceDataOffset);

        if (sourceDataOffset > 0) {
            // Cache serialized
            u32 cachedSerialized = serializedOffset;

            serializedOffset = allocatedOffset;

            *data = dataMemory + allocatedOffset;

            // Reserve memory
            AllocateStatic(*size);

            char* sourceData = blobMemory + cachedSerialized + sourceDataOffset - 4;
            memcpy(*data, sourceData, *size);
            // Restore serialized
            serializedOffset = cachedSerialized;
        }
        else {
            *data = nullptr;
            size = 0;
        }
    }
    else {
        // Data --> Blob
        // Data will be copied at the end of the current blob
        i32 dataOffset = allocatedOffset - serializedOffset;
        Serialize(&dataOffset);

        u32 cachedSerialized = serializedOffset;
        // Move serialization to at the end of the blob.
        serializedOffset = allocatedOffset;
        // Allocate memory in the blob
        AllocateStatic(*size);

        char* destinationData = blobMemory + serializedOffset;
        memcpy(destinationData, *data, *size);

        // Restore serialized
        serializedOffset = cachedSerialized;
    }
}

void BlobSerializer::Serialize(cstring data) {
    // sizet len = strlen( data );
    GASSERTM(false, "To be implemented!");
}

char* BlobSerializer::AllocateStatic(sizet size) {
    if (allocatedOffset + size > totalSize)
    {
        GPrint("Blob allocation error: allocated, requested, total - %u + %u > %u\n", allocatedOffset, size, totalSize);
        return nullptr;
    }

    u32 offset = allocatedOffset;
    allocatedOffset += (u32)size;

    return isReading ? dataMemory + offset : blobMemory + offset;
}

void BlobSerializer::Serialize(RelativeString* data) {

    if (isReading) {
        // Blob --> Data
        Serialize(&data->size);

        i32 sourceDataOffset;
        Serialize(&sourceDataOffset);

        if (sourceDataOffset > 0) {
            // Cache serialized
            u32 cachedSerialized = serializedOffset;

            serializedOffset = allocatedOffset;

            data->data.offset = GetRelativeDataOffset(data) - 4;

            // Reserve memory + string ending
            AllocateStatic((sizet)data->size + 1);

            char* sourceData = blobMemory + cachedSerialized + sourceDataOffset - 4;
            memcpy((char*)data->c_str(), sourceData, (sizet)data->size + 1);
            GPrint("Found %s\n", data->c_str());
            // Restore serialized
            serializedOffset = cachedSerialized;
        }
        else {
            data->SetEmpty();
        }
    }
    else {
        // Data --> Blob
        Serialize(&data->size);
        // Data will be copied at the end of the current blob
        i32 dataOffset = allocatedOffset - serializedOffset;
        Serialize(&dataOffset);

        u32 cachedSerialized = serializedOffset;
        // Move serialization to at the end of the blob.
        serializedOffset = allocatedOffset;
        // Allocate memory in the blob
        AllocateStatic((sizet)data->size + 1);

        char* destinationData = blobMemory + serializedOffset;
        memcpy(destinationData, (char*)data->c_str(), (sizet)data->size + 1);
        GPrint("Written %s, Found %s\n", data->c_str(), destinationData);

        // Restore serialized
        serializedOffset = cachedSerialized;
    }
}

void BlobSerializer::AllocateAndSet(RelativeString& string, cstring format, ...) {

    u32 cachedOffset = allocatedOffset;

    char* destinationMemory = isReading ? dataMemory : blobMemory;

    va_list args;
    va_start(args, format);
#if (_MSC_VER)
    int writtenChars = vsnprintf_s(&destinationMemory[allocatedOffset], totalSize - allocatedOffset, _TRUNCATE, format, args);
#else
    int writtenChars = vsnprintf(&destinationMemory[allocatedOffset], totalSize - allocatedOffset, format, args);
#endif
    allocatedOffset += writtenChars > 0 ? writtenChars : 0;
    va_end(args);

    if (writtenChars < 0) {
        GPrint("New string too big for current buffer! Please allocate more size.\n");
    }

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    destinationMemory[allocatedOffset] = 0;
    ++allocatedOffset;

    string.Set(destinationMemory + cachedOffset, writtenChars);
}

void BlobSerializer::AllocateAndSet(RelativeString& string, char* text, u32 length) {

    if (allocatedOffset + length > totalSize) {
        GPrint("New string too big for current buffer! Please allocate more size.\n");
        return;
    }
    u32 cachedOffset = allocatedOffset;

    char* destinationMemory = isReading ? dataMemory : blobMemory;
    memcpy(&destinationMemory[allocatedOffset], text, length);

    allocatedOffset += length;

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    destinationMemory[allocatedOffset] = 0;
    ++allocatedOffset;

    string.Set(destinationMemory + cachedOffset, length);
}

i32 BlobSerializer::GetRelativeDataOffset(void* data) {
    // dataMemory points to the newly allocated data structure to be used at runtime.
    const i32 dataOffsetFromStart = (i32)((char*)data - dataMemory);
    const i32 dataOffset = allocatedOffset - dataOffsetFromStart;
    return dataOffset;
}

