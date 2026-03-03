#include "TextureLoader.hpp"
#include "ImmediateSubmit.hpp"
#include "Buffer.hpp"
#include "../../BasicServices/Log.h"

#include <stb_image.h>
#include <cmath>
#include <algorithm>

using services::Log;

TextureLoader::TextureLoader(vk::raii::Device&         device,
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
//  load — stb_image → staging buffer → device-local Image with mipmaps →
//         ImageView (all mip levels) + Sampler
// =============================================================================
Texture TextureLoader::load(const std::string& path) {
    // -------------------------------------------------------------------------
    // 1. Load pixels with stb_image
    // -------------------------------------------------------------------------
    int texWidth = 0, texHeight = 0, texChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(),
                                 &texWidth, &texHeight, &texChannels,
                                 STBI_rgb_alpha);
    if (!pixels) {
        Log::Error("Failed to load texture '%s': %s", path.c_str(), stbi_failure_reason());
        return {};
    }

    // -------------------------------------------------------------------------
    // 2. Calculate mip levels + check format supports linear blitting
    // -------------------------------------------------------------------------
    const uint32_t mipLevels = static_cast<uint32_t>(
        std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    auto formatProps = physDevice.getFormatProperties(vk::Format::eR8G8B8A8Srgb);
    const bool linearBlitSupported =
        (formatProps.optimalTilingFeatures &
         vk::FormatFeatureFlagBits::eSampledImageFilterLinear) ==
        vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
    if (!linearBlitSupported) {
        Log::Warn("TextureLoader: linear blit not supported for eR8G8B8A8Srgb "
                  "— falling back to eNearest for mip generation.");
    }
    const vk::Filter blitFilter = linearBlitSupported
                                    ? vk::Filter::eLinear
                                    : vk::Filter::eNearest;

    // -------------------------------------------------------------------------
    // 3. Staging buffer + upload + free CPU pixels
    // -------------------------------------------------------------------------
    const vk::DeviceSize imageSize =
        static_cast<vk::DeviceSize>(texWidth * texHeight * 4);

    Buffer staging{allocator, imageSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT};
    staging.upload(pixels, static_cast<size_t>(imageSize));
    stbi_image_free(pixels);

    // -------------------------------------------------------------------------
    // 4. Device-local Image (VMA) — needs eTransferSrc for blit source
    // -------------------------------------------------------------------------
    Texture tex;
    tex.image = Image{allocator,
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled,
        vk::SampleCountFlagBits::e1,
        mipLevels};

    // -------------------------------------------------------------------------
    // 5. Single-shot GPU work: upload mip 0 + generate mip chain
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
                .baseMipLevel   = 0, .levelCount     = mipLevels,
                .baseArrayLayer = 0, .layerCount     = 1,
            },
        };
        cmd.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &allMips,
        });

        // Copy pixel data into mip level 0
        tex.image.recordCopyFromBuffer(cmd, staging.get());

        // Generate mip levels 1..mipLevels-1 via sequential blits
        int32_t mipWidth  = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; ++i) {
            // Transition mip i-1: eTransferDstOptimal → eTransferSrcOptimal
            vk::ImageMemoryBarrier2 toSrc{
                .srcStageMask        = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask       = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask        = vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask       = vk::AccessFlagBits2::eTransferRead,
                .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
                .newLayout           = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image               = tex.image.get(),
                .subresourceRange    = {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = i - 1, .levelCount     = 1,
                    .baseArrayLayer = 0,     .layerCount     = 1,
                },
            };
            cmd.pipelineBarrier2(vk::DependencyInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &toSrc,
            });

            // Blit mip i-1 (src) → mip i (dst)
            int32_t nextWidth  = mipWidth  > 1 ? mipWidth  / 2 : 1;
            int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

            // ArrayWrapper1D doesn't accept brace-init with VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
            // — set offsets via operator[] instead.
            vk::ImageBlit blit{};
            blit.srcSubresource = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .mipLevel       = i - 1,
                .baseArrayLayer = 0, .layerCount = 1,
            };
            blit.srcOffsets[0] = vk::Offset3D{.x = 0,         .y = 0,          .z = 0};
            blit.srcOffsets[1] = vk::Offset3D{.x = mipWidth,  .y = mipHeight,  .z = 1};
            blit.dstSubresource = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .mipLevel       = i,
                .baseArrayLayer = 0, .layerCount = 1,
            };
            blit.dstOffsets[0] = vk::Offset3D{.x = 0,         .y = 0,          .z = 0};
            blit.dstOffsets[1] = vk::Offset3D{.x = nextWidth, .y = nextHeight, .z = 1};

            cmd.blitImage(
                tex.image.get(), vk::ImageLayout::eTransferSrcOptimal,
                tex.image.get(), vk::ImageLayout::eTransferDstOptimal,
                blit, blitFilter);

            // Transition mip i-1: eTransferSrcOptimal → eShaderReadOnlyOptimal
            vk::ImageMemoryBarrier2 toRead{
                .srcStageMask        = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask       = vk::AccessFlagBits2::eTransferRead,
                .dstStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask       = vk::AccessFlagBits2::eShaderRead,
                .oldLayout           = vk::ImageLayout::eTransferSrcOptimal,
                .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image               = tex.image.get(),
                .subresourceRange    = {
                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = i - 1, .levelCount     = 1,
                    .baseArrayLayer = 0,     .layerCount     = 1,
                },
            };
            cmd.pipelineBarrier2(vk::DependencyInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &toRead,
            });

            mipWidth  = nextWidth;
            mipHeight = nextHeight;
        }

        // Transition the last mip level: eTransferDstOptimal → eShaderReadOnlyOptimal
        vk::ImageMemoryBarrier2 lastMip{
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
                .baseMipLevel   = mipLevels - 1, .levelCount     = 1,
                .baseArrayLayer = 0,             .layerCount     = 1,
            },
        };
        cmd.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &lastMip,
        });
    });

    // -------------------------------------------------------------------------
    // 6. ImageView — expose all mip levels
    // -------------------------------------------------------------------------
    tex.imageView = device.createImageView(vk::ImageViewCreateInfo{
        .image    = tex.image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format   = vk::Format::eR8G8B8A8Srgb,
        .components = {
            .r = vk::ComponentSwizzle::eIdentity,
            .g = vk::ComponentSwizzle::eIdentity,
            .b = vk::ComponentSwizzle::eIdentity,
            .a = vk::ComponentSwizzle::eIdentity,
        },
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0, .levelCount     = mipLevels,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    });

    // -------------------------------------------------------------------------
    // 7. Sampler — linear mipmap filtering, maxLod = mipLevels
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
        .maxLod                  = static_cast<float>(mipLevels),
        .borderColor             = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = false,
    });

    Log::Info("Texture loaded: '%s' (%dx%d, %u mip levels, max anisotropy %.1f).",
              path.c_str(), texWidth, texHeight, mipLevels,
              props.limits.maxSamplerAnisotropy);
    return tex;
}
