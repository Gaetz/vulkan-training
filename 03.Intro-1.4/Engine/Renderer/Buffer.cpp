#include "Buffer.hpp"

// =============================================================================
//  Constructor — allocates VkBuffer + VmaAllocation via VMA
//
//  vk::BufferCreateInfo has the same memory layout as VkBufferCreateInfo
//  (standard_layout struct, same field order). We reinterpret_cast to pass
//  it to VMA's C API, then wrap the resulting VkBuffer in vk::Buffer.
// =============================================================================
Buffer::Buffer(VmaAllocator             allocator,
               vk::DeviceSize           size,
               vk::BufferUsageFlags     usage,
               VmaMemoryUsage           memUsage,
               VmaAllocationCreateFlags allocFlags)
    : allocator(allocator), bufferSize(size)
{
    vk::BufferCreateInfo bufInfo{
        .size        = size,
        .usage       = usage,
        .sharingMode = vk::SharingMode::eExclusive,
    };

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = memUsage;
    allocCreateInfo.flags = allocFlags;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    vmaCreateBuffer(allocator,
                    reinterpret_cast<const VkBufferCreateInfo*>(&bufInfo),
                    &allocCreateInfo,
                    &rawBuffer, &allocation, &allocInfo);
    buffer = vk::Buffer{rawBuffer};
}

// =============================================================================
//  Destructor
// =============================================================================
Buffer::~Buffer() {
    destroy();
}

// =============================================================================
//  Move constructor / assignment
// =============================================================================
Buffer::Buffer(Buffer&& other) noexcept
    : allocator(other.allocator)
    , buffer(other.buffer)
    , allocation(other.allocation)
    , allocInfo(other.allocInfo)
    , bufferSize(other.bufferSize)
{
    other.allocator  = VK_NULL_HANDLE;
    other.buffer     = nullptr;
    other.allocation = VK_NULL_HANDLE;
    other.allocInfo  = {};
    other.bufferSize = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        destroy();
        allocator  = other.allocator;
        buffer     = other.buffer;
        allocation = other.allocation;
        allocInfo  = other.allocInfo;
        bufferSize = other.bufferSize;
        other.allocator  = VK_NULL_HANDLE;
        other.buffer     = nullptr;
        other.allocation = VK_NULL_HANDLE;
        other.allocInfo  = {};
        other.bufferSize = 0;
    }
    return *this;
}

// =============================================================================
//  upload — memcpy into this buffer
//  Uses the persistently-mapped pointer when available (MAPPED_BIT),
//  otherwise calls vmaMapMemory / vmaUnmapMemory.
// =============================================================================
void Buffer::upload(const void* src, size_t bytes) {
    void* mapped  = allocInfo.pMappedData;
    bool  tempMap = (mapped == nullptr);
    if (tempMap)
        vmaMapMemory(allocator, allocation, &mapped);
    memcpy(mapped, src, bytes);
    if (tempMap)
        vmaUnmapMemory(allocator, allocation);
}

// =============================================================================
//  destroy — idempotent
//  vmaDestroyBuffer releases both the VkBuffer and the VmaAllocation.
// =============================================================================
void Buffer::destroy() {
    if (buffer && allocator != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator,
                         static_cast<VkBuffer>(buffer),
                         allocation);
        buffer     = nullptr;
        allocation = VK_NULL_HANDLE;
        allocInfo  = {};
        bufferSize = 0;
    }
    allocator = VK_NULL_HANDLE;
}
