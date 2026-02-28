#pragma once

// vulkan_raii.hpp provides RAII wrappers for all Vulkan handles.
// It includes vulkan.hpp internally, so no need to include it separately.
// VULKAN_HPP_NO_STRUCT_CONSTRUCTORS uses designated-initializer style for structs.
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>

#include <vector>

#include "../../Engine/IRenderer.hpp"

// Step 00 — Setup
// Covers the Vulkan tutorial sections:
//   - 03 / 00_Setup        / 00_Base_code
//   - 03 / 01_Presentation / 00_Window_surface
//   - 03 / 01_Presentation / 01_Swap_chain
//   - 03 / 01_Presentation / 02_Image_views
//
// vk-bootstrap handles creation and device selection.
// All resulting Vulkan handles are stored as vk::raii::* objects which call
// the correct vkDestroy* function automatically when they go out of scope.
//
// Member declaration order determines auto-destruction order (reverse):
//   imageViews → swapchain → device → surface → debugMessenger → instance → context
//
// VMA has no raii wrapper and is the only object requiring explicit cleanup.

class Step00Renderer : public IRenderer {
public:
    bool init(SDL_Window* window) override;
    void render(IScene& scene) override;
    void cleanup() override;

    // Needed to destroy VMA before the RAII device destructor fires.
    ~Step00Renderer() override;

private:
    // --- RAII Vulkan objects ---
    vk::raii::Context                context;
    vk::raii::Instance               instance{nullptr};
    vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};
    vk::raii::SurfaceKHR             surface{nullptr};
    vk::raii::PhysicalDevice         physDevice{nullptr};
    vk::raii::Device                 device{nullptr};
    vk::raii::SwapchainKHR           swapchain{nullptr};
    std::vector<vk::raii::ImageView> imageViews;

    // --- Queues (not owned by the app, no RAII needed) ---
    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    vk::Queue transferQueue;
    uint32_t  graphicsQueueFamily       = 0;
    uint32_t  presentQueueFamily        = 0;
    uint32_t  transferQueueFamily       = 0;
    bool      hasDedicatedTransferQueue = false;

    // --- Swapchain metadata ---
    vk::Format             swapchainFormat = vk::Format::eUndefined;
    vk::Extent2D           swapchainExtent = {};
    std::vector<vk::Image> swapchainImages; // owned by swapchain, not raii

    // --- VMA (no vulkan.hpp RAII support) ---
    VmaAllocator allocator = VK_NULL_HANDLE;

    bool initVulkan(SDL_Window* window);
    bool initSwapchain(SDL_Window* window);
    bool initImageViews();
    bool initAllocator();
};
