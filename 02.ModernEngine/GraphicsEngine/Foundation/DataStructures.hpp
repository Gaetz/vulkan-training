#pragma once

#include "Memory.hpp"
#include "Assert.hpp"

struct ResourcePool {

    void Init(Allocator* allocator, u32 poolSize, u32 resourceSize);
    void Shutdown();

    // Returns an index to the resource
    u32 ObtainResource();      

    void ReleaseResource(u32 index);
    void FreeAllResources();

    void* AccessResource(u32 index);
    const void* AccessResource(u32 index) const;

    u8* memory = nullptr;
    u32* freeIndices = nullptr;
    Allocator* allocator = nullptr;

    u32 freeIndicesHead = 0;
    u32 poolSize = 16;
    u32 resourceSize = 4;
    u32 usedIndices = 0;

}; // struct ResourcePool

//
//
template <typename T>
struct ResourcePoolTyped : public ResourcePool {

    void Init(Allocator* allocator, u32 poolSize);
    void Shutdown();

    T* Obtain();
    void Release(T* resource);

    T* Get(u32 index);
    const T* Get(u32 index) const;

}; // struct ResourcePoolTyped

template<typename T>
inline void ResourcePoolTyped<T>::Init(Allocator* allocator_, u32 poolSize_) {
    ResourcePool::Init(allocator_, poolSize_, sizeof(T));
}

template<typename T>
inline void ResourcePoolTyped<T>::Shutdown() {
    if (freeIndicesHead != 0) {
        GPrint("Resource pool has unfreed resources.\n");

        for (u32 i = 0; i < freeIndicesHead; ++i) {
            GPrint("\tResource %u, %s\n", freeIndices[i], Get(freeIndices[i])->name);
        }
    }
    ResourcePool::Shutdown();
}

template<typename T>
inline T* ResourcePoolTyped<T>::Obtain() {
    u32 resourceIndex = ResourcePool::ObtainResource();
    if (resourceIndex != u32Max) {
        T* resource = Get(resourceIndex);
        resource->poolIndex = resourceIndex;
        return resource;
    }

    return nullptr;
}

template<typename T>
inline void ResourcePoolTyped<T>::Release(T* resource) {
    ResourcePool::ReleaseResource(resource->poolIndex);
}

template<typename T>
inline T* ResourcePoolTyped<T>::Get(u32 index) {
    return (T*)ResourcePool::AccessResource(index);
}

template<typename T>
inline const T* ResourcePoolTyped<T>::Get(u32 index) const {
    return (const T*)ResourcePool::AccessResource(index);
}
