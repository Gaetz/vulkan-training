#pragma once

// vk::Buffer is a non-owning vulkan.hpp handle (same layout as VkBuffer).
// We deliberately do NOT use vk::raii::Buffer because VMA manages both the
// VkBuffer AND the VmaAllocation together via vmaDestroyBuffer; using
// vk::raii::Buffer would call vkDestroyBuffer and leave the allocation leaked.
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

// GPU buffer managed by VMA.
//
// Two typical usages:
//   Staging (CPU→GPU upload):
//     usage      = eTransferSrc
//     memUsage   = VMA_MEMORY_USAGE_AUTO
//     allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
//                | VMA_ALLOCATION_CREATE_MAPPED_BIT
//
//   Device-local (GPU-only, optimal):
//     usage      = eVertexBuffer | eTransferDst  (or eIndexBuffer | …)
//     memUsage   = VMA_MEMORY_USAGE_AUTO
//     allocFlags = 0
class Buffer {
public:
    Buffer() = default;
    Buffer(VmaAllocator              allocator,
           vk::DeviceSize            size,
           vk::BufferUsageFlags      usage,
           VmaMemoryUsage            memUsage   = VMA_MEMORY_USAGE_AUTO,
           VmaAllocationCreateFlags  allocFlags = 0);

    ~Buffer();

    Buffer(const Buffer&)            = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    [[nodiscard]] vk::Buffer     get()   const { return buffer; }
    [[nodiscard]] vk::DeviceSize size()  const { return bufferSize; }
    [[nodiscard]] bool           valid() const { return !!buffer; }

    // Copy `bytes` bytes from `src` into this buffer.
    // Works for both persistently-mapped (MAPPED_BIT) and non-mapped allocations.
    void upload(const void* src, size_t bytes);

    // Explicit destruction — safe to call multiple times.
    void destroy();

private:
    VmaAllocator      allocator  = VK_NULL_HANDLE;
    vk::Buffer        buffer     = {};          // non-owning vulkan.hpp handle
    VmaAllocation     allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo  = {};
    vk::DeviceSize    bufferSize = 0;
};
