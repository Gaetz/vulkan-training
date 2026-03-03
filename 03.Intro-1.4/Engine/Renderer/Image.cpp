#include "Image.hpp"

// =============================================================================
//  Constructor — allocates VkImage + VmaAllocation via VMA
//
//  vk::ImageCreateInfo has the same memory layout as VkImageCreateInfo
//  (standard_layout struct). We reinterpret_cast to pass it to VMA's C API,
//  then wrap the resulting VkImage in vk::Image.
// =============================================================================
Image::Image(VmaAllocator        allocator,
             uint32_t            width,
             uint32_t            height,
             vk::Format          format,
             vk::ImageUsageFlags usage)
    : allocator(allocator), width(width), height(height), format(format)
{
    vk::ImageCreateInfo imgInfo{
        .imageType     = vk::ImageType::e2D,
        .format        = format,
        .extent        = {width, height, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = vk::SampleCountFlagBits::e1,
        .tiling        = vk::ImageTiling::eOptimal,
        .usage         = usage,
        .sharingMode   = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage rawImage = VK_NULL_HANDLE;
    vmaCreateImage(allocator,
                   reinterpret_cast<const VkImageCreateInfo*>(&imgInfo),
                   &allocCreateInfo,
                   &rawImage, &allocation, nullptr);
    image = vk::Image{rawImage};
}

// =============================================================================
//  Destructor
// =============================================================================
Image::~Image() {
    destroy();
}

// =============================================================================
//  Move constructor / assignment
// =============================================================================
Image::Image(Image&& other) noexcept
    : allocator(other.allocator)
    , image(other.image)
    , allocation(other.allocation)
    , width(other.width)
    , height(other.height)
    , format(other.format)
{
    other.allocator  = VK_NULL_HANDLE;
    other.image      = nullptr;
    other.allocation = VK_NULL_HANDLE;
    other.width      = 0;
    other.height     = 0;
    other.format     = vk::Format::eUndefined;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        destroy();
        allocator  = other.allocator;
        image      = other.image;
        allocation = other.allocation;
        width      = other.width;
        height     = other.height;
        format     = other.format;
        other.allocator  = VK_NULL_HANDLE;
        other.image      = nullptr;
        other.allocation = VK_NULL_HANDLE;
        other.width      = 0;
        other.height     = 0;
        other.format     = vk::Format::eUndefined;
    }
    return *this;
}

// =============================================================================
//  recordTransitionLayout — records a layout-transition barrier into cmd.
//  Uses synchronization2 (Vulkan 1.3 core).
//  srcQueueFamilyIndex / dstQueueFamilyIndex are vk::QueueFamilyIgnored (~0u);
//  with VULKAN_HPP_NO_STRUCT_CONSTRUCTORS the default is 0, not ~0u.
// =============================================================================
void Image::recordTransitionLayout(vk::CommandBuffer cmd,
                                    vk::ImageLayout   oldLayout,
                                    vk::ImageLayout   newLayout) {
    vk::ImageMemoryBarrier2 barrier{
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0, .levelCount     = 1,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    };

    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe;
        barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
        barrier.dstStageMask  = vk::PipelineStageFlagBits2::eTransfer;
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
               newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTransfer;
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    }
    // Unsupported transitions: validation layers will report the error.

    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    });
}

// =============================================================================
//  recordCopyFromBuffer — records a buffer→image copy into cmd.
//  The image must already be in eTransferDstOptimal layout.
// =============================================================================
void Image::recordCopyFromBuffer(vk::CommandBuffer cmd, vk::Buffer buffer) {
    vk::BufferImageCopy region{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };
    cmd.copyBufferToImage(buffer, image,
                          vk::ImageLayout::eTransferDstOptimal,
                          region);
}

// =============================================================================
//  destroy — idempotent
//  vmaDestroyImage releases both the VkImage and the VmaAllocation.
// =============================================================================
void Image::destroy() {
    if (image && allocator != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator,
                        static_cast<VkImage>(image),
                        allocation);
        image      = nullptr;
        allocation = VK_NULL_HANDLE;
    }
    allocator = VK_NULL_HANDLE;
}
