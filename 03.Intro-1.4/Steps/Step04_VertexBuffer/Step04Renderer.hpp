#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

#include "../../Engine/Renderer/IRenderer.hpp"
#include "../../Engine/Renderer/Buffer.hpp"
#include "../../Engine/Renderer/ShaderLoader.hpp"
#include "../../Engine/Renderer/ImmediateSubmit.hpp"

// Step 04 — Vertex buffer + staging buffer + index buffer
class Step04Renderer : public IRenderer {
public:
    // -------------------------------------------------------------------------
    // Vertex layout — one binding, two attributes (position + color)
    // -------------------------------------------------------------------------
    struct Vertex {
        glm::vec2 pos;
        glm::vec3 color;

        static vk::VertexInputBindingDescription getBindingDescription() {
            return vk::VertexInputBindingDescription{
                .binding   = 0,
                .stride    = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex,
            };
        }

        static std::array<vk::VertexInputAttributeDescription, 2>
        getAttributeDescriptions() {
            return {{
                {.location = 0,
                 .binding  = 0,
                 .format   = vk::Format::eR32G32Sfloat,
                 .offset   = offsetof(Vertex, pos)},
                {.location = 1,
                 .binding  = 0,
                 .format   = vk::Format::eR32G32B32Sfloat,
                 .offset   = offsetof(Vertex, color)},
            }};
        }
    };

    bool init(SDL_Window* window) override;
    void render(IScene& scene) override;
    void cleanup() override;
    void onWindowResized() override { framebufferResized = true; }

    ~Step04Renderer() override;

private:
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
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     inFlightFences;

    // --- GPU buffers (destroyed before VMA allocator) ---
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount = 0;

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
    bool initVertexBuffer();
    bool initIndexBuffer();

    void cleanupSwapchain();
    void recreateSwapchain();

    void destroyVMAResources();
    void copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);
    void recordCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex);
};
