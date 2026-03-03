#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <string>

#include "Model.hpp"

// Loads an OBJ file and uploads the geometry to the GPU as a Model.
//
// Usage:
//   Model model = ModelLoader{device, allocator, commandPool, graphicsQueue}
//                     .load("assets/models/viking_room.obj");
//
// The loader stores references — it must not outlive its arguments.
// Vertex layout: float3 pos, float3 color (white), float2 texCoord (V-flipped).
class ModelLoader {
public:
    ModelLoader(vk::raii::Device&      device,
                VmaAllocator           allocator,
                vk::raii::CommandPool& commandPool,
                vk::raii::Queue&       queue);

    [[nodiscard]] Model load(const std::string& path);

private:
    vk::raii::Device&      device;
    VmaAllocator           allocator;
    vk::raii::CommandPool& commandPool;
    vk::raii::Queue&       queue;
};
