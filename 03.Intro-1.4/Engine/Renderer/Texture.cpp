#include "Texture.hpp"

// =============================================================================
//  Move constructor / assignment — defaulted (all members are movable)
// =============================================================================
Texture::Texture(Texture&&) noexcept = default;
Texture& Texture::operator=(Texture&&) noexcept = default;

// =============================================================================
//  destroy — explicit, idempotent.
//  Order matters: sampler and imageView are RAII handles that must be cleared
//  before the underlying VkImage (managed by VMA via Image::destroy()) is freed.
// =============================================================================
void Texture::destroy() {
    sampler.clear();
    imageView.clear();
    image.destroy();
}
