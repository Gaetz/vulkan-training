#include "KtxTextureLoader.hpp"
#include "ImmediateSubmit.hpp"
#include "Buffer.hpp"
#include "../../BasicServices/Log.h"

#include <ktx.h>
#include <algorithm>

using services::Log;

KtxTextureLoader::KtxTextureLoader(vk::raii::Device&         device,
                                   vk::raii::PhysicalDevice& physDevice,
                                   VmaAllocator              allocator,
                                   vk::raii::CommandPool&    commandPool,
                                   vk::raii::Queue&          queue)
    : device{device}
    , physDevice{physDevice}
    , allocator{allocator}
    , commandPool{commandPool}
    , queue{queue}
{}

// =============================================================================
//  load — libktx → staging buffer → device-local Image (pre-baked mips) →
//         ImageView (all mip levels) + Sampler
// =============================================================================
Texture KtxTextureLoader::load(const std::string& path) {
    // -------------------------------------------------------------------------
    // 1. Load KTX2 file
    // -------------------------------------------------------------------------
    ktxTexture* kTexture = nullptr;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        path.c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &kTexture);

    if (result != KTX_SUCCESS) {
        Log::Error("KtxTextureLoader: failed to load '%s' (KTX error %d).",
                   path.c_str(), static_cast<int>(result));
        return {};
    }

    // -------------------------------------------------------------------------
    // 2. Read metadata + pre-compute per-mip offsets before destroying texture
    // -------------------------------------------------------------------------
    const uint32_t baseWidth  = kTexture->baseWidth;
    const uint32_t baseHeight = kTexture->baseHeight;
    const uint32_t numLevels  = kTexture->numLevels;

    // Determine Vulkan image format from the KTX2 header (VkFormat is a uint32_t enum).
    vk::Format format = vk::Format::eR8G8B8A8Srgb; // fallback for KTX1
    if (kTexture->classId == ktxTexture2_c) {
        format = static_cast<vk::Format>(
            reinterpret_cast<ktxTexture2*>(kTexture)->vkFormat);
    }

    // Collect byte offsets of each mip level within the ktx data blob.
    std::vector<ktx_size_t> mipOffsets(numLevels);
    for (uint32_t i = 0; i < numLevels; ++i)
        ktxTexture_GetImageOffset(kTexture, i, 0, 0, &mipOffsets[i]);

    // -------------------------------------------------------------------------
    // 3. Staging buffer — all mip levels in one contiguous upload
    // -------------------------------------------------------------------------
    const vk::DeviceSize totalSize =
        static_cast<vk::DeviceSize>(ktxTexture_GetDataSize(kTexture));

    Buffer staging{allocator, totalSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT};
    staging.upload(ktxTexture_GetData(kTexture), static_cast<size_t>(totalSize));

    ktxTexture_Destroy(kTexture);

    // -------------------------------------------------------------------------
    // 4. Device-local Image — eTransferDst | eSampled, all mip levels
    // -------------------------------------------------------------------------
    Texture tex;
    tex.image = Image{allocator,
        baseWidth, baseHeight,
        format,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled,
        vk::SampleCountFlagBits::e1,
        numLevels};

    // -------------------------------------------------------------------------
    // 5. Single-shot GPU work: transition all mips, then copy each from staging
    // -------------------------------------------------------------------------
    ImmediateSubmit{device, commandPool, queue}([&](vk::CommandBuffer cmd) {
        // Transition ALL mip levels eUndefined → eTransferDstOptimal
        vk::ImageMemoryBarrier2 allMips{
            .srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe,
            .srcAccessMask       = vk::AccessFlagBits2::eNone,
            .dstStageMask        = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask       = vk::AccessFlagBits2::eTransferWrite,
            .oldLayout           = vk::ImageLayout::eUndefined,
            .newLayout           = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image               = tex.image.get(),
            .subresourceRange    = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0, .levelCount     = numLevels,
                .baseArrayLayer = 0, .layerCount     = 1,
            },
        };
        cmd.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &allMips,
        });

        // Copy each mip level from the staging buffer.
        for (uint32_t i = 0; i < numLevels; ++i) {
            const uint32_t mipWidth  = std::max(1u, baseWidth  >> i);
            const uint32_t mipHeight = std::max(1u, baseHeight >> i);

            vk::BufferImageCopy region{
                .bufferOffset      = static_cast<vk::DeviceSize>(mipOffsets[i]),
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .mipLevel       = i,
                    .baseArrayLayer = 0, .layerCount = 1,
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = {mipWidth, mipHeight, 1},
            };
            cmd.copyBufferToImage(
                staging.get(), tex.image.get(),
                vk::ImageLayout::eTransferDstOptimal, region);
        }

        // Transition ALL mip levels eTransferDstOptimal → eShaderReadOnlyOptimal
        vk::ImageMemoryBarrier2 toRead{
            .srcStageMask        = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask       = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
            .dstAccessMask       = vk::AccessFlagBits2::eShaderRead,
            .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
            .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image               = tex.image.get(),
            .subresourceRange    = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0, .levelCount     = numLevels,
                .baseArrayLayer = 0, .layerCount     = 1,
            },
        };
        cmd.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &toRead,
        });
    });

    staging.destroy();

    // -------------------------------------------------------------------------
    // 6. ImageView — expose all mip levels, format from KTX file
    // -------------------------------------------------------------------------
    tex.imageView = device.createImageView(vk::ImageViewCreateInfo{
        .image    = tex.image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format   = tex.image.getFormat(),
        .components = {
            .r = vk::ComponentSwizzle::eIdentity,
            .g = vk::ComponentSwizzle::eIdentity,
            .b = vk::ComponentSwizzle::eIdentity,
            .a = vk::ComponentSwizzle::eIdentity,
        },
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0, .levelCount     = numLevels,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    });

    // -------------------------------------------------------------------------
    // 7. Sampler — linear mipmap filtering, maxLod = numLevels
    // -------------------------------------------------------------------------
    auto props = physDevice.getProperties();
    tex.sampler = device.createSampler(vk::SamplerCreateInfo{
        .magFilter               = vk::Filter::eLinear,
        .minFilter               = vk::Filter::eLinear,
        .mipmapMode              = vk::SamplerMipmapMode::eLinear,
        .addressModeU            = vk::SamplerAddressMode::eRepeat,
        .addressModeV            = vk::SamplerAddressMode::eRepeat,
        .addressModeW            = vk::SamplerAddressMode::eRepeat,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = true,
        .maxAnisotropy           = props.limits.maxSamplerAnisotropy,
        .compareEnable           = false,
        .compareOp               = vk::CompareOp::eAlways,
        .minLod                  = 0.0f,
        .maxLod                  = static_cast<float>(numLevels),
        .borderColor             = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = false,
    });

    Log::Info("KtxTexture loaded: '%s' (%ux%u, %u mips, format %s, max aniso %.1f).",
              path.c_str(), baseWidth, baseHeight, numLevels,
              vk::to_string(format).c_str(),
              props.limits.maxSamplerAnisotropy);
    return tex;
}
