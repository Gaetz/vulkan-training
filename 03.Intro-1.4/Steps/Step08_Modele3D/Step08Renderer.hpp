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
#include "../../Engine/Renderer/Image.hpp"
#include "../../Engine/Renderer/Model.hpp"
#include "../../Engine/Renderer/ModelLoader.hpp"
#include "../../Engine/Renderer/Texture.hpp"
#include "../../Engine/Renderer/ShaderLoader.hpp"

// Step 08 — Loading Models: OBJ loaded via tinyobjloader, deduplicated into
// a Model (vertex buffer + index buffer, uint32_t indices).
class Step08Renderer : public IRenderer {
public:
    // -------------------------------------------------------------------------
    // Vertex layout — same as Step 07 (float3 pos, float3 color, float2 uv).
    // operator== and hash are needed for vertex deduplication via unordered_map.
    // -------------------------------------------------------------------------
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color && texCoord == other.texCoord;
        }

        static vk::VertexInputBindingDescription getBindingDescription() {
            return vk::VertexInputBindingDescription{
                .binding   = 0,
                .stride    = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex,
            };
        }

        static std::array<vk::VertexInputAttributeDescription, 3>
        getAttributeDescriptions() {
            return {{
                {.location = 0, .binding = 0,
                 .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
                {.location = 1, .binding = 0,
                 .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
                {.location = 2, .binding = 0,
                 .format = vk::Format::eR32G32Sfloat,    .offset = offsetof(Vertex, texCoord)},
            }};
        }
    };

    // -------------------------------------------------------------------------
    // Uniform buffer object — one per frame in flight
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

    ~Step08Renderer() override;

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

    // --- Descriptor set layout ---
    vk::raii::DescriptorSetLayout descriptorSetLayout{nullptr};

    // --- Graphics pipeline ---
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline       graphicsPipeline{nullptr};

    // --- Texture ---
    Texture texture;

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

    // --- 3D mesh (VMA vertex + index buffers) ---
    Model model;

    // --- Depth image (VMA-managed; swapchain-sized, recreated on resize) ---
    Image               depthImage;
    vk::raii::ImageView depthImageView{nullptr};
    vk::Format          depthFormat = vk::Format::eUndefined;

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
    bool initCommandBuffers();
    bool initSyncObjects();
    bool initModel();
    bool initDepthResources();
    bool initTexture();
    bool initDescriptors();

    vk::Format findDepthFormat();

    void cleanupSwapchain();
    void recreateSwapchain();

    void destroyVMAResources();
    void recordCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex);
};
