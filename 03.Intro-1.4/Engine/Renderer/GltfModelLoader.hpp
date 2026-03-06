#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <string>

#include "Model.hpp"

// Loads a GLTF (.gltf) or GLB (.glb) file and uploads the first mesh's geometry
// to the GPU as a Model. Uses tinygltf internally.
//
// Usage:
//   Model model = GltfModelLoader{device, allocator, commandPool, graphicsQueue}
//                     .load("assets/models/viking_room.gltf");
//
// The loader stores references — it must not outlive its arguments.
// Vertex layout: float3 pos, float3 color (white), float2 texCoord.
// Only the first mesh / first primitive is loaded.
class GltfModelLoader {
public:
    GltfModelLoader(vk::raii::Device&      device,
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
