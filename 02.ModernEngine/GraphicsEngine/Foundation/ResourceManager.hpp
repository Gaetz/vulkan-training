#pragma once

#include "Platform.hpp"
#include "Assert.hpp"
#include "HashMap.hpp"


struct ResourceManager;

//
// Reference counting and named resource.
struct Resource
{

    void AddReference() { ++references; }
    void RemoveReference()
    {
        GASSERT(references != 0);
        --references;
    }

    u64 references = 0;
    cstring name = nullptr;
}; // struct Resource

//
//
struct ResourceCompiler
{

}; // struct ResourceCompiler

//
//
struct ResourceLoader
{

    virtual Resource* Get(cstring name) = 0;
    virtual Resource* Get(u64 hashed_name) = 0;

    virtual Resource* Unload(cstring name) = 0;

    virtual Resource* CreateFromFile(cstring name, cstring filename, ResourceManager* resourceManager) { return nullptr; }

}; // struct ResourceLoader

//
//
struct ResourceFilenameResolver
{

    virtual cstring GetBinaryPathFromName(cstring name) = 0;

}; // struct ResourceFilenameResolver

//
//
struct ResourceManager
{

    void Init(Allocator* allocator, ResourceFilenameResolver* resolver);
    void shutdown();

    template <typename T>
    T* Load(cstring name);

    template <typename T>
    T* Get(cstring name);

    template <typename T>
    T* Get(u64 hashedName);

    template <typename T>
    T* Reload(cstring name);

    void SetLoader(cstring resourceType, ResourceLoader* loader);
    void SetCompiler(cstring resourceType, ResourceCompiler* compiler);

    FlatHashMap<u64, ResourceLoader*> loaders;
    FlatHashMap<u64, ResourceCompiler*> compilers;

    Allocator* allocator;
    ResourceFilenameResolver* filenameResolver;

}; // struct ResourceManager

template <typename T>
inline T* ResourceManager::Load(cstring name)
{
    ResourceLoader* loader = loaders.Get(T::k_type_hash);
    if (loader)
    {
        // Search if the resource is already in cache
        T* resource = (T*)loader->Get(name);
        if (resource)
            return resource;

        // Resource not in cache, create from file
        cstring path = filenameResolver->GetBinaryPathFromName(name);
        return (T*)loader->CreateFromFile(name, path, this);
    }
    return nullptr;
}

template <typename T>
inline T* ResourceManager::Get(cstring name)
{
    ResourceLoader* loader = loaders.Get(T::typeHash);
    if (loader)
    {
        return (T*)loader->Get(name);
    }
    return nullptr;
}

template <typename T>
inline T* ResourceManager::Get(u64 hashedName)
{
    ResourceLoader* loader = loaders.Get(T::typeHash);
    if (loader)
    {
        return (T*)loader->Get(hashedName);
    }
    return nullptr;
}

template <typename T>
inline T* ResourceManager::Reload(cstring name)
{
    ResourceLoader* loader = loaders.Get(T::typeHash);
    if (loader)
    {
        T* resource = (T*)loader->Get(name);
        if (resource)
        {
            loader->Unload(name);

            // Resource not in cache, create from file
            cstring path = filenameResolver->GetBinaryPathFromName(name);
            return (T*)loader->CreateFromFile(name, path, this);
        }
    }
    return nullptr;
}
