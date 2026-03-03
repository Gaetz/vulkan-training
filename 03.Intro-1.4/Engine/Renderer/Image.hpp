#pragma once

// vk::Image is a non-owning vulkan.hpp handle (same layout as VkImage).
// We deliberately do NOT use vk::raii::Image because VMA manages both the
// VkImage AND the VmaAllocation together via vmaDestroyImage; using
// vk::raii::Image would call vkDestroyImage and leave the allocation leaked.
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

// GPU image managed by VMA (device-local, tiling = eOptimal).
//
// Typical usage — texture image:
//   usage = eTransferDst | eSampled
//   Fill via a staging Buffer + single-time command buffer:
//
//   auto cmd = beginSingleTimeCommands();
//   img.recordTransitionLayout(*cmd, eUndefined, eTransferDstOptimal);
//   img.recordCopyFromBuffer(*cmd, staging.get());
//   img.recordTransitionLayout(*cmd, eTransferDstOptimal, eShaderReadOnlyOptimal);
//   endSingleTimeCommands(std::move(cmd));
class Image {
public:
    Image() = default;
    Image(VmaAllocator        allocator,
          uint32_t            width,
          uint32_t            height,
          vk::Format          format,
          vk::ImageUsageFlags usage);

    ~Image();

    Image(const Image&)            = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) noexcept;
    Image& operator=(Image&&) noexcept;

    [[nodiscard]] vk::Image  get()       const { return image; }
    [[nodiscard]] bool       valid()     const { return !!image; }
    [[nodiscard]] uint32_t   getWidth()  const { return width; }
    [[nodiscard]] uint32_t   getHeight() const { return height; }
    [[nodiscard]] vk::Format getFormat() const { return format; }

    // Records a layout transition barrier into cmd.
    // Supports:  eUndefined → eTransferDstOptimal
    //            eTransferDstOptimal → eShaderReadOnlyOptimal
    // Uses synchronization2 (vk::PipelineStageFlagBits2).
    // srcQueueFamilyIndex / dstQueueFamilyIndex are set to vk::QueueFamilyIgnored.
    void recordTransitionLayout(vk::CommandBuffer cmd,
                                vk::ImageLayout   oldLayout,
                                vk::ImageLayout   newLayout);

    // Records a buffer → image copy into cmd.
    // The image must be in eTransferDstOptimal layout.
    // Uses the stored width/height as the copy extent.
    void recordCopyFromBuffer(vk::CommandBuffer cmd, vk::Buffer buffer);

    // Explicit destruction — safe to call multiple times.
    void destroy();

private:
    VmaAllocator  allocator  = VK_NULL_HANDLE;
    vk::Image     image      = nullptr;     // non-owning vulkan.hpp handle
    VmaAllocation allocation = VK_NULL_HANDLE;
    uint32_t      width      = 0;
    uint32_t      height     = 0;
    vk::Format    format     = vk::Format::eUndefined;
};
