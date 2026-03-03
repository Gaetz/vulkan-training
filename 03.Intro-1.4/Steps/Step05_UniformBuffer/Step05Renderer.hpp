#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>

#include <array>
#include <string>
#include <vector>

#include "../../Engine/Renderer/IRenderer.hpp"
#include "../../Engine/Renderer/Buffer.hpp"
#include "../../Engine/Renderer/ShaderLoader.hpp"
#include "../../Engine/Renderer/ImmediateSubmit.hpp"

// Step 05 — Uniform buffer: MVP matrices passed to the vertex shader via a UBO.
class Step05Renderer : public IRenderer {
public:
    // -------------------------------------------------------------------------
    // Vertex layout — unchanged from Step 04
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
                {.location = 0, .binding = 0,
                 .format = vk::Format::eR32G32Sfloat,   .offset = offsetof(Vertex, pos)},
                {.location = 1, .binding = 0,
                 .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
            }};
        }
    };

    // -------------------------------------------------------------------------
    // Uniform buffer object — one per frame in flight
    // alignas(16) matches Vulkan's std140 requirement for mat4 members.
    // -------------------------------------------------------------------------
    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    bool init(SDL_Window* window) override;
    void render(IScene& scene) override;
    void cleanup() override;
    void onWindowResized() override { framebufferResized = true; }

    ~Step05Renderer() override;

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

    // --- Descriptor set layout (created before pipeline layout) ---
    vk::raii::DescriptorSetLayout    descriptorSetLayout{nullptr};

    // --- Graphics pipeline ---
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline       graphicsPipeline{nullptr};

    // --- Descriptor pool + sets (non-owning; pool owns the sets) ---
    vk::raii::DescriptorPool       descriptorPool{nullptr};
    std::vector<vk::DescriptorSet> descriptorSets;

    // --- Command pool & buffers ---
    vk::raii::CommandPool                commandPool{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    // --- Sync objects ---
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     inFlightFences;

    // --- GPU buffers (destroyed before VMA allocator) ---
    Buffer   vertexBuffer;
    Buffer   indexBuffer;
    uint32_t indexCount = 0;

    // --- Uniform buffers — one per frame in flight, persistently mapped ---
    std::vector<Buffer> uniformBuffers;

    // --- Frame tracking ---
    uint32_t currentFrame       = 0;
    bool     framebufferResized = false;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

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

    bool initVulkan(SDL_Window* window);
    bool initSwapchain(SDL_Window* window);
    bool initImageViews();
    bool initAllocator();
    bool initPipeline();
    bool initDescriptors();
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
