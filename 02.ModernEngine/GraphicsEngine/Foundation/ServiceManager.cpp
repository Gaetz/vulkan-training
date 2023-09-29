#include "ServiceManager.hpp"

static ServiceManager serviceManager;
ServiceManager* ServiceManager::instance = &serviceManager;

void ServiceManager::Init(Allocator* allocator_) {

    GPrint("ServiceManager init\n");
    allocator = allocator_;

    services.Init(allocator, 8);
}

void ServiceManager::Shutdown() {

    services.Shutdown();

    GPrint("ServiceManager shutdown\n");
}

void ServiceManager::AddService(Service* service, cstring name) {
    u64 hashName = HashCalculate(name);
    FlatHashMapIterator it = services.find(hashName);
    GASSERTM(it.isInvalid(), "Overwriting service %s, is this intended ?", name);
    services.Insert(hashName, service);
}

void ServiceManager::RemoveService(cstring name) {
    u64 hash_name = HashCalculate(name);
    services.Remove(hash_name);
}

Service* ServiceManager::GetService(cstring name) {
    u64 hash_name = HashCalculate(name);
    return services.Get(hash_name);
}