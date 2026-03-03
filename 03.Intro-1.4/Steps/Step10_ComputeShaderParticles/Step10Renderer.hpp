#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>

#include <array>
#include <vector>

#include "../../Engine/Renderer/IRenderer.hpp"
#include "../../Engine/Renderer/Buffer.hpp"
#include "../../Engine/Renderer/ShaderLoader.hpp"

// Step 10 — Compute Shader Particles
// GPU particle system: compute shader updates positions, graphics pipeline renders
// particles as point sprites. Demonstrates compute pipeline, SSBO double-buffering,
// and compute↔graphics synchronization.
class Step10Renderer : public IRenderer {
public:
    // -------------------------------------------------------------------------
    // Particle layout — must match Slang struct and Step10Scene::Particle
    // stride = 32 bytes: position(8) + velocity(8) + color(16)
    // -------------------------------------------------------------------------
    struct Particle {
        glm::vec2 position;  // offset  0
        glm::vec2 velocity;  // offset  8  (skipped by graphics vertex input)
        glm::vec4 color;     // offset 16

        static vk::VertexInputBindingDescription getBindingDescription() {
            return vk::VertexInputBindingDescription{
                .binding   = 0,
                .stride    = sizeof(Particle),
                .inputRate = vk::VertexInputRate::eVertex,
            };
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
            return {{
                {.location = 0, .binding = 0,
                 .format = vk::Format::eR32G32Sfloat,
                 .offset = static_cast<uint32_t>(offsetof(Particle, position))},
                {.location = 1, .binding = 0,
                 .format = vk::Format::eR32G32B32A32Sfloat,
                 .offset = static_cast<uint32_t>(offsetof(Particle, color))},
            }};
        }
    };

    struct DeltaTimeUBO {
        alignas(16) float deltaTime;
    };

    static constexpr uint32_t PARTICLE_COUNT       = 16384; // 64 * 256
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    bool init(SDL_Window* window) override;
    void render(IScene& scene) override;
    void cleanup() override;
    void onWindowResized() override { framebufferResized = true; }
    void waitIdle() override { if (*device) device.waitIdle(); }

    // Called by Engine after scene->init()
    void onSceneReady(IScene& scene) override;

    ~Step10Renderer() override;

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

    // --- Compute pipeline ---
    vk::raii::DescriptorSetLayout computeDescriptorSetLayout{nullptr};
    vk::raii::PipelineLayout      computePipelineLayout{nullptr};
    vk::raii::Pipeline            computePipeline{nullptr};

    // --- Graphics pipeline (empty layout — no descriptors) ---
    vk::raii::PipelineLayout graphicsPipelineLayout{nullptr};
    vk::raii::Pipeline       graphicsPipeline{nullptr};

    // --- Descriptors ---
    vk::raii::DescriptorPool       descriptorPool{nullptr};
    std::vector<vk::DescriptorSet> computeDescriptorSets; // [MAX_FRAMES_IN_FLIGHT]

    // --- Particle SSBOs and delta-time UBOs ---
    std::vector<Buffer> particleBuffers;  // [MAX_FRAMES_IN_FLIGHT]: eStorageBuffer|eVertexBuffer
    std::vector<Buffer> deltaTimeBuffers; // [MAX_FRAMES_IN_FLIGHT]: eUniformBuffer, host-mapped

    // --- Command pool and buffers ---
    vk::raii::CommandPool                commandPool{nullptr};
    std::vector<vk::raii::CommandBuffer> computeCommandBuffers;
    std::vector<vk::raii::CommandBuffer> graphicsCommandBuffers;

    // --- Sync objects ---
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;  // [MAX_FRAMES_IN_FLIGHT]
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;  // [swapchainImages.size()]
    std::vector<vk::raii::Semaphore> computeFinishedSemaphores; // [MAX_FRAMES_IN_FLIGHT]
    std::vector<vk::raii::Fence>     graphicsInFlightFences;    // [MAX_FRAMES_IN_FLIGHT]
    std::vector<vk::raii::Fence>     computeInFlightFences;     // [MAX_FRAMES_IN_FLIGHT]

    // --- Frame tracking ---
    uint32_t currentFrame       = 0;
    bool     framebufferResized = false;

    // --- Queues ---
    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::Queue presentQueue{nullptr};
    uint32_t graphicsQueueFamily = 0;
    uint32_t presentQueueFamily  = 0;

    // --- Swapchain metadata ---
    vk::Format             swapchainFormat = vk::Format::eUndefined;
    vk::Extent2D           swapchainExtent = {};
    std::vector<vk::Image> swapchainImages;

    // --- VMA ---
    VmaAllocator allocator = VK_NULL_HANDLE;

    // --- Init helpers ---
    bool initVulkan(SDL_Window* w);
    bool initSwapchain(SDL_Window* w);
    bool initImageViews();
    bool initAllocator();
    bool initPipelines();
    bool initCommandBuffers();
    bool initSyncObjects();
    bool initParticleBuffers(const std::vector<Particle>& particles);
    bool initDescriptors();

    void cleanupSwapchain();
    void recreateSwapchain();
    void destroyVMAResources();

    void recordComputeCommandBuffer(vk::raii::CommandBuffer& cmd);
    void recordGraphicsCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex);
};
