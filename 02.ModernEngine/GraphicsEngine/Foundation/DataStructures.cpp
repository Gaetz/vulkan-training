#include "DataStructures.hpp"

#include <string.h>


static const u32 k_invalid_index = 0xffffffff;

// Resource Pool ////////////////////////////////////////////////////////////////

void ResourcePool::Init(Allocator* allocator_, u32 poolSize_, u32 resourceSize_) {

    allocator = allocator_;
    poolSize = poolSize_;
    resourceSize = resourceSize_;

    // Group allocate ( resource size + u32 )
    sizet allocationSize_ = poolSize * (resourceSize + sizeof(u32));
    memory = (u8*)allocator->Allocate(allocationSize_, 1);
    memset(memory, 0, allocationSize_);

    // Allocate and add free indices
    freeIndices = (u32*)(memory + poolSize * resourceSize);
    freeIndicesHead = 0;

    for (u32 i = 0; i < poolSize; ++i) {
        freeIndices[i] = i;
    }

    usedIndices = 0;
}

void ResourcePool::Shutdown() {

    if (freeIndicesHead != 0) {
        GPrint("Resource pool has unfreed resources.\n");

        for (u32 i = 0; i < freeIndicesHead; ++i) {
            GPrint("\tResource %u\n", freeIndices[i]);
        }
    }

    GASSERT(usedIndices == 0);

    allocator->Deallocate(memory);
}

void ResourcePool::FreeAllResources() {
    freeIndicesHead = 0;
    usedIndices = 0;

    for (uint32_t i = 0; i < poolSize; ++i) {
        freeIndices[i] = i;
    }
}

u32 ResourcePool::ObtainResource() {
    // TODO: add bits for checking if resource is alive and use bitmasks.
    if (freeIndicesHead < poolSize) {
        const u32 freeIndex = freeIndices[freeIndicesHead++];
        ++usedIndices;
        return freeIndex;
    }
    // Error: no more resources left!
    GASSERT(false);
    return k_invalid_index;
}

void ResourcePool::ReleaseResource(u32 handle) {
    // TODO: add bits for checking if resource is alive and use bitmasks.
    freeIndices[--freeIndicesHead] = handle;
    --usedIndices;
}

void* ResourcePool::AccessResource(u32 handle) {
    if (handle != k_invalid_index) {
        return &memory[handle * resourceSize];
    }
    return nullptr;
}

const void* ResourcePool::AccessResource(u32 handle) const {
    if (handle != k_invalid_index) {
        return &memory[handle * resourceSize];
    }
    return nullptr;
}


