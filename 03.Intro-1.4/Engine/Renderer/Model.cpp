#include "Model.hpp"

// =============================================================================
//  Constructor — takes ownership of pre-built device-local buffers.
// =============================================================================
Model::Model(Buffer vb, Buffer ib, uint32_t ic)
    : vertexBuffer{std::move(vb)}
    , indexBuffer{std::move(ib)}
    , indexCount{ic}
{}

// =============================================================================
//  Move constructor / assignment — defaulted (all members are movable)
// =============================================================================
Model::Model(Model&&) noexcept            = default;
Model& Model::operator=(Model&&) noexcept = default;

// =============================================================================
//  destroy — explicit, idempotent.
// =============================================================================
void Model::destroy() {
    vertexBuffer.destroy();
    indexBuffer.destroy();
    indexCount = 0;
}

// =============================================================================
//  upload — create device-local vertex + index buffers from raw data.
//  Both buffer copies are issued in a single ImmediateSubmit.
// =============================================================================
Model Model::upload(VmaAllocator allocator,
                    vk::raii::Device& device,
                    vk::raii::CommandPool& commandPool,
                    vk::raii::Queue& queue,
                    const void* vertexData, size_t vbSize,
                    const void* indexData,  size_t ibSize,
                    uint32_t indexCount)
{
    Buffer stagingVB{allocator, vbSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT};
    stagingVB.upload(vertexData, vbSize);

    Buffer stagingIB{allocator, ibSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT};
    stagingIB.upload(indexData, ibSize);

    Buffer vb{allocator, vbSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO};
    Buffer ib{allocator, ibSize,
        vk::BufferUsageFlagBits::eIndexBuffer  | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO};

    ImmediateSubmit{device, commandPool, queue}([&](vk::CommandBuffer cmd) {
        cmd.copyBuffer(stagingVB.get(), vb.get(), vk::BufferCopy{.size = vbSize});
        cmd.copyBuffer(stagingIB.get(), ib.get(), vk::BufferCopy{.size = ibSize});
    });

    return Model{std::move(vb), std::move(ib), indexCount};
}
