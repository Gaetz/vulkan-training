#pragma once
#include "Array.hpp"
#include "HashMap.hpp"


struct Service;

struct ServiceManager {

    void Init(Allocator* allocator);
    void Shutdown();

    void AddService(Service* service, cstring name);
    void RemoveService(cstring name);

    Service* GetService(cstring name);

    template<typename T>
    T* Get();

    static ServiceManager* instance;

    FlatHashMap<u64, Service*> services;
    Allocator* allocator = nullptr;

}; // struct ServiceManager

template<typename T>
inline T* ServiceManager::Get() {
    T* service = (T*)GetService(T::name);
    if (!service) {
        AddService(T::instance(), T::name);
    }

    return T::instance();
}