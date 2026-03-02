#pragma once

// vulkan_raii.hpp provides RAII wrappers for all Vulkan handles.
// It includes vulkan.hpp internally, so no need to include it separately.
// VULKAN_HPP_NO_STRUCT_CONSTRUCTORS uses designated-initializer style for structs.
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>

#include <string>
#include <vector>

#include "../../Engine/IRenderer.hpp"

// Step 01 — Basic Pipeline
class Step01Renderer : public IRenderer {
public:
    bool init(SDL_Window* window) override;
    void render(IScene& scene) override;
    void cleanup() override;

    // Needed to destroy VMA before the RAII device destructor fires.
    ~Step01Renderer() override;

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

    // --- Graphics pipeline ---
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline       graphicsPipeline{nullptr};

    // --- Queues (owned by the device, destroyed with it — no explicit cleanup needed) ---
    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::Queue presentQueue{nullptr};
    vk::raii::Queue transferQueue{nullptr};
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
    bool initPipeline();
    vk::raii::ShaderModule loadShaderModule(const std::string& path);
};
