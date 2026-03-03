#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <string>

#include "Texture.hpp"

// Loads a texture from disk and uploads it to the GPU.
//
// Stores references — must not outlive its constructor arguments.
// Usage:
//   Texture t = TextureLoader{device, physDevice, allocator, commandPool, queue}
//                   .load("assets/textures/foo.jpg");
class TextureLoader {
public:
    TextureLoader(vk::raii::Device&         device,
                  vk::raii::PhysicalDevice& physDevice,
                  VmaAllocator              allocator,
                  vk::raii::CommandPool&    commandPool,
                  vk::raii::Queue&          queue);

    // Load image from path, upload to GPU, create image view and sampler.
    // Returns a default-constructed (invalid) Texture on failure.
    [[nodiscard]] Texture load(const std::string& path);

private:
    vk::raii::Device&         device;
    vk::raii::PhysicalDevice& physDevice;
    VmaAllocator              allocator;
    vk::raii::CommandPool&    commandPool;
    vk::raii::Queue&          queue;
};
