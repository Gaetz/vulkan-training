#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>

#include <string>
#include <vector>

#include "../../Engine/IRenderer.hpp"

// Step 03 — Swapchain recreation on resize / out-of-date
class Step03Renderer : public IRenderer {
public:
    bool init(SDL_Window* window) override;
    void render(IScene& scene) override;
    void cleanup() override;
    void onWindowResized() override { framebufferResized = true; }

    ~Step03Renderer() override;

private:
    // --- SDL window (needed to query size during recreation) ---
    SDL_Window* window = nullptr;

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

    // --- Command pool & buffers ---
    vk::raii::CommandPool                commandPool{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    // --- Sync objects ---
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores; // per frame slot
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores; // per swapchain image
    std::vector<vk::raii::Fence>     inFlightFences;           // per frame slot

    // --- Frame tracking ---
    uint32_t currentFrame       = 0;
    bool     framebufferResized = false;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // --- Queues ---
    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::Queue presentQueue{nullptr};
    vk::raii::Queue transferQueue{nullptr};
    uint32_t graphicsQueueFamily       = 0;
    uint32_t presentQueueFamily        = 0;
    uint32_t transferQueueFamily       = 0;
    bool     hasDedicatedTransferQueue = false;

    // --- Swapchain metadata ---
    vk::Format             swapchainFormat = vk::Format::eUndefined;
    vk::Extent2D           swapchainExtent = {};
    std::vector<vk::Image> swapchainImages;

    // --- VMA ---
    VmaAllocator allocator = VK_NULL_HANDLE;

    bool initVulkan(SDL_Window* window);
    bool initSwapchain(SDL_Window* window);
    bool initImageViews();
    bool initAllocator();
    bool initPipeline();
    bool initCommandBuffers();
    bool initSyncObjects();

    void cleanupSwapchain();
    void recreateSwapchain();

    void recordCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex);
    vk::raii::ShaderModule loadShaderModule(const std::string& path);
};
