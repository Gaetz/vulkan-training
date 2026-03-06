#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <string>

#include "Texture.hpp"

// Loads a KTX2 texture from disk and uploads it to the GPU.
// Pre-baked mipmaps stored in the file are uploaded directly — no GPU blit is needed.
//
// Stores references — must not outlive its constructor arguments.
// Usage:
//   Texture t = KtxTextureLoader{device, physDevice, allocator, commandPool, queue}
//                   .load("assets/textures/foo.ktx2");
//
// Convert assets with the Vulkan SDK's toktx tool:
//   toktx --t2 --mipmap --assign_oetf srgb output.ktx2 input.png
class KtxTextureLoader {
public:
    KtxTextureLoader(vk::raii::Device&         device,
                     vk::raii::PhysicalDevice& physDevice,
                     VmaAllocator              allocator,
                     vk::raii::CommandPool&    commandPool,
                     vk::raii::Queue&          queue);

    // Load KTX2 image from path, upload to GPU with pre-baked mip chain,
    // create image view and sampler.
    // Returns a default-constructed (invalid) Texture on failure.
    [[nodiscard]] Texture load(const std::string& path);

private:
    vk::raii::Device&         device;
    vk::raii::PhysicalDevice& physDevice;
    VmaAllocator              allocator;
    vk::raii::CommandPool&    commandPool;
    vk::raii::Queue&          queue;
};
