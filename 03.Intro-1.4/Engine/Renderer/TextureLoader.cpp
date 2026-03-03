#include "TextureLoader.hpp"
#include "ImmediateSubmit.hpp"
#include "Buffer.hpp"
#include "../../BasicServices/Log.h"

#include <stb_image.h>

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
//  load — stb_image → staging buffer → device-local Image → ImageView + Sampler
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
    // 2. Staging buffer + upload + free CPU pixels
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
    // 3. Device-local Image (VMA)
    // -------------------------------------------------------------------------
    Texture tex;
    tex.image = Image{allocator,
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight),
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled};

    // -------------------------------------------------------------------------
    // 4. Single-shot GPU upload: transition → copy → transition
    // -------------------------------------------------------------------------
    ImmediateSubmit{device, commandPool, queue}([&](vk::CommandBuffer cmd) {
        tex.image.recordTransitionLayout(cmd,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal);
        tex.image.recordCopyFromBuffer(cmd, staging.get());
        tex.image.recordTransitionLayout(cmd,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal);
    });

    // -------------------------------------------------------------------------
    // 5. ImageView
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
            .baseMipLevel   = 0, .levelCount     = 1,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    });

    // -------------------------------------------------------------------------
    // 6. Sampler (anisotropy = device max)
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
        .maxLod                  = 0.0f,
        .borderColor             = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = false,
    });

    Log::Info("Texture loaded: '%s' (%dx%d RGBA, max anisotropy %.1f).",
              path.c_str(), texWidth, texHeight, props.limits.maxSamplerAnisotropy);
    return tex;
}
