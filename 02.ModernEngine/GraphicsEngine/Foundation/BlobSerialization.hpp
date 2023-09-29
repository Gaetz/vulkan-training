#pragma once

#include "Platform.hpp"
#include "Memory.hpp"
#include "Assert.hpp"
#include "Array.hpp"
#include "RelativeDataStructure.hpp"

#include "Blob.hpp"

// Defines:
// G_BLOB_WRITE         - use it in code that can write blueprints, like data compilers.
   

struct Allocator;

struct BlobSerializer {

    // Allocate size bytes, set the data version and start writing.
    // Data version will be saved at the beginning of the file.
    template <typename T>
    T* WriteAndPrepare(Allocator* allocator, u32 serializerVersion, sizet size);

    template <typename T>
    void WriteAndSerialize(Allocator* allocator, u32 serializerVersion, sizet size, T* root_data);

    void WriteCommon(Allocator* allocator, u32 serializerVersion, sizet size);

    // Init blob in reading mode from a chunk of preallocated memory.
    // Size is used to check wheter reading is happening outside of the chunk.
    // Allocator is used to allocate memory if needed (for example when reading an array).
    template <typename T>
    T* Read(Allocator* allocator, u32 serializerVersion, sizet size, char* blobMemory, bool forceSerialization_ = false);

    void Shutdown();

    // Methods used both for reading and writing.
    // Leaf of the serialization code.
    void Serialize(char* data);
    void Serialize(i8* data);
    void Serialize(u8* data);
    void Serialize(i16* data);
    void Serialize(u16* data);
    void Serialize(i32* data);
    void Serialize(u32* data);
    void Serialize(i64* data);
    void Serialize(u64* data);
    void Serialize(f32* data);
    void Serialize(f64* data);
    void Serialize(bool* data);
    void Serialize(cstring data);

    void SerializeMemory(void* data, sizet size);
    void SerializeMemoryBlock(void** data, u32* size);

    template <typename T>
    void Serialize(RelativePointer<T>* data);

    template <typename T>
    void Serialize(RelativeArray<T>* data);

    template <typename T>
    void Serialize(Array<T>* data);

    template <typename T>
    void Serialize(T* data);

    void Serialize(RelativeString* data);

    // Static allocation from the blob allocated memory.
    char* AllocateStatic(sizet size);  // Just allocate size bytes and return. Used to fill in structures.

    template <typename T>
    T* AllocateStatic();

    template <typename T>
    void AllocateAndSet(RelativePointer<T>& data, void* sourceData = nullptr);

    template <typename T>
    void AllocateAndSet(RelativeArray<T>& data, u32 numElements, void* sourceData = nullptr);       // Allocate an array and set it so it can be accessed.

    void AllocateAndSet(RelativeString& string, cstring format, ...);            // Allocate and set a static string.
    void AllocateAndSet(RelativeString& string, char* text, u32 length);      // Allocate and set a static string.

    i32 GetRelativeDataOffset(void* data);

    char* blobMemory = nullptr;
    char* dataMemory = nullptr;

    Allocator* allocator = nullptr;

    u32 totalSize = 0;
    u32 serializedOffset = 0;
    u32 allocatedOffset = 0;

    u32 serializerVersion = 0xffffffff; // Version coming from the code.
    u32 dataVersion = 0xffffffff; // Version read from blob or written into blob.

    u32 isReading = 0;
    u32 isMappable = 0;

    u32 hasAllocatedMemory = 0;

}; // struct BlobSerializer

// Implementations/////////////////////////////////////////////////////////

// BlobSerializer /////////////////////////////////////////////////////////////

template<typename T>
T* BlobSerializer::WriteAndPrepare(Allocator* allocator_, u32 serializerVersion_, sizet size) {

    WriteCommon(allocator_, serializerVersion_, size);

    // Allocate root data. BlobHeader is already allocated in the WriteCommon method.
    AllocateStatic(sizeof(T) - sizeof(BlobHeader));

    // Manually manage blob serialization.
    dataMemory = nullptr;

    return (T*)blobMemory;
}


template<typename T>
void BlobSerializer::WriteAndSerialize(Allocator* allocator_, u32 serializerVersion_, sizet size, T* data) {

    RASSERT(data); // Should always have data passed as parameter!

    WriteCommon(allocator_, serializerVersion_, size);

    // Allocate root data. BlobHeader is already allocated in the WriteCommon method.
    AllocateStatic(sizeof(T) - sizeof(BlobHeader));

    // Save root data memory for offset calculation
    dataMemory = (char*)data;
    // Serialize root data
    serialize(data);
}

template<typename T>
T* BlobSerializer::Read(Allocator* allocator_, u32 serializerVersion_, sizet size, char* blobMemory_, bool forceSerialization_) {

    allocator = allocator_;
    blobMemory = blobMemory_;
    dataMemory = nullptr;

    totalSize = (u32)size;
    serializedOffset = allocatedOffset = 0;

    serializerVersion = serializerVersion_;
    isReading = 1;
    hasAllocatedMemory = 0;

    // Read header from blob.
    BlobHeader* header = (BlobHeader*)blobMemory;
    dataVersion = header->version;
    isMappable = header->mappable;

    // If serializer and data are at the same version, no need to serialize.
    // TODO: is mappable should be taken in consideration.
    if (serializerVersion == dataVersion && !forceSerialization_) {
        return (T*)(blobMemory);
    }

    hasAllocatedMemory = 1;
    serializerVersion = dataVersion;

    // Allocate data
    dataMemory = (char*)GAllocaM(size, allocator);
    T* destination_data = (T*)dataMemory;

    serializedOffset += sizeof(BlobHeader);

    AllocateStatic(sizeof(T));
    // Read from blob to data
    serialize(destination_data);

    return destination_data;
}

template<typename T>
inline void BlobSerializer::AllocateAndSet(RelativePointer<T>& data, void* sourceData) {
    char* destination_memory = AllocateStatic(sizeof(T));
    data.Set(destination_memory);

    if (sourceData) {
        MemoryCopy(destination_memory, sourceData, sizeof(T));
    }
}

template<typename T>
inline void BlobSerializer::AllocateAndSet(RelativeArray<T>& data, u32 numElements, void* sourceData) {
    char* destination_memory = AllocateStatic(sizeof(T) * numElements);
    data.Set(destination_memory, numElements);

    if (sourceData) {
        MemoryCopy(destination_memory, sourceData, sizeof(T) * numElements);
    }
}

template<typename T>
inline T* BlobSerializer::AllocateStatic() {
    return (T*)AllocateStatic(sizeof(T));
}

template<typename T>
inline void BlobSerializer::Serialize(RelativePointer<T>* data) {

    if (isReading) {
        // READING!
        // Blob --> Data
        i32 sourceDataOffset;
        Serialize(&sourceDataOffset);

        // Early out to not follow null pointers.
        if (sourceDataOffset == 0) {
            data->offset = 0;
            return;
        }

        data->offset = GetRelativeDataOffset(data);

        // Allocate memory and set pointer
        AllocateStatic<T>();

        // Cache source serialized offset.
        u32 cachedSerialized = serializedOffset;
        // Move serialization offset.
        // The offset is still "this->offset", and the serialized offset
        // points just right AFTER it, thus move back by sizeof(offset).
        serializedOffset = cachedSerialized + sourceDataOffset - sizeof(u32);
        // Serialize/visit the pointed data structure
        serialize(data->Get());
        // Restore serialization offset
        serializedOffset = cachedSerialized;
    }
    else {
        // WRITING!
        // Data --> Blob
        // Calculate offset used by RelativePointer.
        // Remember this:
        // char* address = ( ( char* )&this->offset ) + offset;
        // Serialized offset points to what will be the "this->offset"
        // Allocated offset points to the still not allocated memory,
        // Where we will allocate from.
        i32 dataOffset = allocatedOffset - serializedOffset;
        Serialize(&dataOffset);

        // To jump anywhere and correctly restore the serialization process,
        // cache the current serialization offset
        u32 cachedSerialized = serializedOffset;
        // Move serialization to the newly allocated memory at the end of the blob.
        serializedOffset = allocatedOffset;
        // Allocate memory in the blob
        AllocateStatic<T>();
        // Serialize/visit the pointed data structure
        serialize(data->Get());
        // Restore serialized
        serializedOffset = cachedSerialized;
    }
}

template<typename T>
inline void BlobSerializer::Serialize(RelativeArray<T>* data) {

    if (isReading) {
        // Blob --> Data
        Serialize(&data->size);

        i32 sourceDataOffset;
        Serialize(&sourceDataOffset);

        // Cache serialized
        u32 cachedSerialized = serializedOffset;

        data->data.offset = GetRelativeDataOffset(data) - sizeof(u32);

        // Reserve memory
        AllocateStatic(data->size * sizeof(T));

        serializedOffset = cachedSerialized + sourceDataOffset - sizeof(u32);

        for (u32 i = 0; i < data->size; ++i) {
            T* destination = &data->Get()[i];
            serialize(destination);

            destination = destination;
        }
        // Restore serialized
        serializedOffset = cachedSerialized;

    }
    else {
        // Data --> Blob
        Serialize(&data->size);
        // Data will be copied at the end of the current blob
        i32 dataOffset = allocatedOffset - serializedOffset;
        Serialize(&dataOffset);

        u32 cachedSerialized = serializedOffset;
        // Move serialization to the newly allocated memory,
        // at the end of the blob.
        serializedOffset = allocatedOffset;
        // Allocate memory in the blob
        AllocateStatic(data->size * sizeof(T));

        for (u32 i = 0; i < data->size; ++i) {
            T* sourceData = &data->Get()[i];
            serialize(sourceData);

            sourceData = sourceData;
        }
        // Restore serialized
        serializedOffset = cachedSerialized;
    }
}

template<typename T>
inline void BlobSerializer::Serialize(Array<T>* data) {

    if (isReading) {
        // Blob --> Data
        Serialize(&data->size);

        u64 serialization_pad;
        Serialize(&serialization_pad);
        Serialize(&serialization_pad);

        u32 packed_data_offset;
        Serialize(&packed_data_offset);
        i32 sourceDataOffset = (u32)(packed_data_offset & 0x7fffffff);

        // Cache serialized
        u32 cachedSerialized = serializedOffset;

        data->allocator = nullptr;
        data->capacity = data->size;
        //data->relative = ( packedDataOffset >> 31 );
        // Point straight to the end
        data->data = (T*)(dataMemory + allocatedOffset);
        //data->data.offset = GetRelativeDataOffset( data ) - 4;

        // Reserve memory
        AllocateStatic(data->size * sizeof(T));
        // 
        serializedOffset = cachedSerialized + sourceDataOffset - sizeof(u32);// -sizeof( u64 ) * 2;

        for (u32 i = 0; i < data->size; ++i) {
            T* destination = &((*data)[i]);
            serialize(destination);

            destination = destination;
        }
        // Restore serialized
        serializedOffset = cachedSerialized;

    }
    else {
        // Data --> Blob
        Serialize(&data->size);
        // Add serialization pads so that we serialize all bytes of the struct Array.
        u64 serialization_pad = 0;
        Serialize(&serialization_pad);
        Serialize(&serialization_pad);

        // Data will be copied at the end of the current blob
        i32 dataOffset = allocatedOffset - serializedOffset;
        // Set higher bit of flag
        u32 packedDataOffset = ((u32)dataOffset | (1 << 31));
        Serialize(&packedDataOffset);

        u32 cachedSerialized = serializedOffset;
        // Move serialization to the newly allocated memory,
        // at the end of the blob.
        serializedOffset = allocatedOffset;
        // Allocate memory in the blob
        AllocateStatic(data->size * sizeof(T));

        for (u32 i = 0; i < data->size; ++i) {
            T* sourceData = &((*data)[i]);
            serialize(sourceData);

            sourceData = sourceData;
        }
        // Restore serialized
        serializedOffset = cachedSerialized;
    }
}

template<typename T>
inline void BlobSerializer::Serialize(T* data) {
    // Should not arrive here!
    GASSERT(false);
}


