#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "Image.hpp"

// Value type owning the three Vulkan objects needed to bind a texture:
//   Image (VMA-managed), ImageView and Sampler (RAII).
//
// Construction is done exclusively via TextureLoader::load().
// Destruction order: sampler → imageView → image (VMA).
// destroy() is explicit and idempotent; also called by the move-assignment operator.
class Texture {
public:
    Texture() = default;
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    [[nodiscard]] vk::Image     getImage()   const { return image.get(); }
    [[nodiscard]] vk::ImageView getView()    const { return *imageView; }
    [[nodiscard]] vk::Sampler   getSampler() const { return *sampler; }
    [[nodiscard]] bool          valid()      const { return image.valid(); }

    // Explicit destruction — safe to call multiple times.
    // Order: sampler → imageView → image.
    void destroy();

    friend class TextureLoader;
    friend class KtxTextureLoader;

private:
    Image               image;
    vk::raii::ImageView imageView{nullptr};
    vk::raii::Sampler   sampler{nullptr};
};
