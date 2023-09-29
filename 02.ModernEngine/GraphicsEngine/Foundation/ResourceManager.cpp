#include "ResourceManager.hpp"

void ResourceManager::Init(Allocator* allocator_, ResourceFilenameResolver* resolver) {

    this->allocator = allocator_;
    this->filenameResolver = resolver;

    loaders.Init(allocator, 8);
    compilers.Init(allocator, 8);
}

void ResourceManager::shutdown() {

    loaders.Shutdown();
    compilers.Shutdown();
}

void ResourceManager::SetLoader(cstring resourceType, ResourceLoader* loader) {
    const u64 hashedName = HashCalculate(resourceType);
    loaders.Insert(hashedName, loader);
}

void ResourceManager::SetCompiler(cstring resourceType, ResourceCompiler* compiler) {
    const u64 hashedName = HashCalculate(resourceType);
    compilers.Insert(hashedName, compiler);
}