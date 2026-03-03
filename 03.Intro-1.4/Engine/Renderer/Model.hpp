#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include "Buffer.hpp"
#include "ImmediateSubmit.hpp"

// GPU mesh resource: vertex buffer + index buffer + index count.
// Index type is always uint32_t (supports models with > 65535 vertices).
//
// Use Model::upload() to create from raw vertex/index data — handles staging internally.
class Model {
public:
    Model() = default;
    Model(Buffer vertexBuffer, Buffer indexBuffer, uint32_t indexCount);

    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) noexcept;
    Model& operator=(Model&&) noexcept;

    [[nodiscard]] vk::Buffer getVertexBuffer() const { return vertexBuffer.get(); }
    [[nodiscard]] vk::Buffer getIndexBuffer()  const { return indexBuffer.get(); }
    [[nodiscard]] uint32_t   getIndexCount()   const { return indexCount; }
    [[nodiscard]] bool       valid()           const { return vertexBuffer.valid(); }

    // Explicit destruction — safe to call multiple times.
    void destroy();

    // Upload vertex + index data via staging buffers (single GPU submission).
    static Model upload(VmaAllocator           allocator,
                        vk::raii::Device&      device,
                        vk::raii::CommandPool& commandPool,
                        vk::raii::Queue&       queue,
                        const void*  vertexData, size_t vertexDataSize,
                        const void*  indexData,  size_t indexDataSize,
                        uint32_t     indexCount);

private:
    Buffer   vertexBuffer;
    Buffer   indexBuffer;
    uint32_t indexCount = 0;
};
