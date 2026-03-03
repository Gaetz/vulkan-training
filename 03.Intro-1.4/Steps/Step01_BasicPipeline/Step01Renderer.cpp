#include "Step01Renderer.hpp"
#include "../../BasicServices/Log.h"

// vk-bootstrap is only used locally for creation and selection; no vkb type
// is stored as a member — all Vulkan handles are owned by vk::raii::* wrappers.
#include <VkBootstrap.h>
#include <SDL3/SDL_vulkan.h>

#include <array>

using services::Log;

// =============================================================================
//  Vulkan debug messenger callback — routes validation messages to Log
// =============================================================================
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*userData*/)
{
    using S = VkDebugUtilsMessageSeverityFlagBitsEXT;
    if      (severity & S::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        Log::Error("[Vulkan] %s", data->pMessage);
    else if (severity & S::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        Log::Warn("[Vulkan] %s", data->pMessage);
    else if (severity & S::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        Log::Info("[Vulkan] %s", data->pMessage);
    else
        Log::Trace("[Vulkan] %s", data->pMessage);
    return VK_FALSE;
}

// =============================================================================
//  Destructor
//  VMA must be destroyed before the RAII device destructor fires.
//  The destructor body runs before member destructors, so this is safe.
// =============================================================================
Step01Renderer::~Step01Renderer() {
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }
    // imageViews, swapchain, device, surface, debugMessenger, instance, context
    // are all destroyed automatically in reverse declaration order.
}

// =============================================================================
//  IRenderer::init
// =============================================================================
bool Step01Renderer::init(SDL_Window* window) {
    if (!initVulkan(window))    return false;
    if (!initSwapchain(window)) return false;
    if (!initImageViews())      return false;
    if (!initAllocator())       return false;
    if (!initPipeline())        return false;

    Log::Info("Renderer initialized — instance, device, swapchain and image views ready.");
    return true;
}

// =============================================================================
//  IRenderer::render  (empty at this stage)
// =============================================================================
void Step01Renderer::render(IScene& /*scene*/) {}

// =============================================================================
//  IRenderer::cleanup
//  Clears RAII handles in reverse creation order. VMA must come before device.
//  After clear(), every handle is null so the destructor is a no-op.
// =============================================================================
void Step01Renderer::cleanup() {
    graphicsPipeline.clear();
    pipelineLayout.clear();

    imageViews.clear();

    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }

    swapchain.clear();
    device.clear();
    surface.clear();
    debugMessenger.clear();
    instance.clear();

    Log::Info("Renderer cleaned up.");
}

// =============================================================================
//  initVulkan — instance, surface, physical device, logical device, queues
// =============================================================================
bool Step01Renderer::initVulkan(SDL_Window* window) {

    // -------------------------------------------------------------------------
    // 1. Instance (via vk-bootstrap) → wrapped in vk::raii::Instance
    //    context provides the Vulkan loader (vkGetInstanceProcAddr).
    //    The raii wrapper calls vkDestroyInstance in its destructor.
    //    We do NOT call vkb::destroy_instance — that would double-free.
    // -------------------------------------------------------------------------
    auto instanceResult = vkb::InstanceBuilder{}
        .set_app_name("Vulkan Tutorial — Step 00")
        .set_engine_name("No Engine")
        .require_api_version(1, 4, 0)
#ifndef NDEBUG
        .request_validation_layers()
        .set_debug_callback(vulkanDebugCallback)
#endif
        .build();

    if (!instanceResult) {
        Log::Error("Failed to create Vulkan instance: %s",
                   instanceResult.error().message().c_str());
        return false;
    }

    auto vkbInst = instanceResult.value();
    instance = vk::raii::Instance{context, vkbInst.instance};
    Log::Info("Vulkan instance created (API 1.4).");

#ifndef NDEBUG
    // Wrap the debug messenger created by vk-bootstrap.
    if (vkbInst.debug_messenger != VK_NULL_HANDLE) {
        debugMessenger = vk::raii::DebugUtilsMessengerEXT{instance,
                                                          vkbInst.debug_messenger};
    }
#endif

    // -------------------------------------------------------------------------
    // 2. Window surface (SDL) → wrapped in vk::raii::SurfaceKHR
    //    *instance dereferences raii to vk::Instance, then to VkInstance.
    // -------------------------------------------------------------------------
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window,
                                  static_cast<VkInstance>(*instance),
                                  nullptr, &rawSurface)) {
        Log::Error("Failed to create Vulkan surface: %s", SDL_GetError());
        return false;
    }
    surface = vk::raii::SurfaceKHR{instance, rawSurface};
    Log::Info("Window surface created.");

    // -------------------------------------------------------------------------
    // 3. Physical device (vk-bootstrap selection) → vk::raii::PhysicalDevice
    //    PhysicalDevice has no destructor (physical devices are not destroyed).
    // -------------------------------------------------------------------------
    auto physDeviceResult = vkb::PhysicalDeviceSelector{vkbInst}
        .set_surface(rawSurface)
        .set_minimum_version(1, 4)
        // slangc emits OpCapability DrawParameters (BaseVertex) when translating
        // SV_VertexID to HLSL semantics. Mandatory on all Vulkan 1.1+ devices.
        .set_required_features_11(VkPhysicalDeviceVulkan11Features{
            .shaderDrawParameters = VK_TRUE,
        })
        // Core Vulkan 1.3 features required for later rendering steps.
        .set_required_features_13(VkPhysicalDeviceVulkan13Features{
            .synchronization2  = VK_TRUE,
            .dynamicRendering  = VK_TRUE,
        })
        // Only require graphics + present. Transfer detection happens after
        // device creation to support both discrete GPUs (dedicated transfer
        // family) and Apple M1 via MoltenVK (no dedicated family).
        .select();

    if (!physDeviceResult) {
        Log::Error("Failed to select physical device: %s",
                   physDeviceResult.error().message().c_str());
        return false;
    }

    auto vkbPhysDev = physDeviceResult.value();
    physDevice = vk::raii::PhysicalDevice{instance, vkbPhysDev.physical_device};
    Log::Info("Physical device: %s", physDevice.getProperties().deviceName.data());

    // -------------------------------------------------------------------------
    // 4. Logical device (vk-bootstrap) → vk::raii::Device
    // -------------------------------------------------------------------------
    auto deviceResult = vkb::DeviceBuilder{vkbPhysDev}.build();

    if (!deviceResult) {
        Log::Error("Failed to create logical device: %s",
                   deviceResult.error().message().c_str());
        return false;
    }

    auto vkbDev = deviceResult.value();
    device = vk::raii::Device{physDevice, vkbDev.device};
    Log::Info("Logical device created.");

    // -------------------------------------------------------------------------
    // 5. Queues — get family indices from vkb while vkbDev is in scope,
    //    then retrieve queue handles from the raii device.
    //    device.getQueue() returns vk::raii::Queue; we dereference (*) to
    //    extract a plain vk::Queue (queues are not destroyed explicitly).
    // -------------------------------------------------------------------------
    graphicsQueueFamily = vkbDev.get_queue_index(vkb::QueueType::graphics).value();
    presentQueueFamily  = vkbDev.get_queue_index(vkb::QueueType::present).value();
    graphicsQueue = device.getQueue(graphicsQueueFamily, 0);
    presentQueue  = device.getQueue(presentQueueFamily,  0);

    auto tqIdx = vkbDev.get_queue_index(vkb::QueueType::transfer);
    if (tqIdx) {
        transferQueueFamily       = tqIdx.value();
        hasDedicatedTransferQueue = (transferQueueFamily != graphicsQueueFamily);
    } else {
        transferQueueFamily       = graphicsQueueFamily;
        hasDedicatedTransferQueue = false;
    }
    transferQueue = device.getQueue(transferQueueFamily, 0);

    Log::Info("Queues — graphics %u, present %u, transfer %u%s.",
              graphicsQueueFamily, presentQueueFamily, transferQueueFamily,
              hasDedicatedTransferQueue ? " (dedicated)" : " (shared with graphics)");
    return true;
}

// =============================================================================
//  initSwapchain — VkSwapchainKHR + VkImage handles
//
//  Raw handles and queue indices are passed to SwapchainBuilder so it can set
//  up the correct image sharing mode (EXCLUSIVE vs CONCURRENT).
//  The swapchain is wrapped in vk::raii::SwapchainKHR.
//  Images are retrieved via swapchain.getImages() — they stay as vk::Image
//  (not raii) since they are owned and freed by the swapchain itself.
// =============================================================================
bool Step01Renderer::initSwapchain(SDL_Window* window) {
    int fbWidth = 0, fbHeight = 0;
    SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);

    auto swapResult = vkb::SwapchainBuilder{
            static_cast<VkPhysicalDevice>(*physDevice),
            static_cast<VkDevice>(*device),
            static_cast<VkSurfaceKHR>(*surface),
            graphicsQueueFamily,
            presentQueueFamily}
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_extent(static_cast<uint32_t>(fbWidth),
                            static_cast<uint32_t>(fbHeight))
        // image count: default (0) → vkb uses minImageCount + 1
        .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build();

    if (!swapResult) {
        Log::Error("Failed to create swapchain: %s", swapResult.error().message().c_str());
        return false;
    }

    auto vkbSwap    = swapResult.value();
    swapchain       = vk::raii::SwapchainKHR{device, vkbSwap.swapchain};
    swapchainFormat = static_cast<vk::Format>(vkbSwap.image_format);
    swapchainExtent = {vkbSwap.extent.width, vkbSwap.extent.height};

    // swapchain.getImages() queries vkGetSwapchainImagesKHR via the raii wrapper.
    swapchainImages = swapchain.getImages();

    Log::Info("Swapchain created — %ux%u, format %s, %zu images.",
              swapchainExtent.width, swapchainExtent.height,
              vk::to_string(swapchainFormat).c_str(),
              swapchainImages.size());
    return true;
}

// =============================================================================
//  initImageViews — one vk::raii::ImageView per swapchain image
//
//  device.createImageView() returns a vk::raii::ImageView directly — no
//  manual vkDestroyImageView needed. The struct uses the designated-initializer
//  style (VULKAN_HPP_NO_STRUCT_CONSTRUCTORS) matching the tutorial exactly.
// =============================================================================
bool Step01Renderer::initImageViews() {
    for (vk::Image image : swapchainImages) {
        vk::ImageViewCreateInfo viewInfo{
            .image    = image,
            .viewType = vk::ImageViewType::e2D,
            .format   = swapchainFormat,
            .components = {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity,
            },
            .subresourceRange = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        imageViews.push_back(device.createImageView(viewInfo));
    }

    Log::Info("%zu image views created.", imageViews.size());
    return true;
}

// =============================================================================
//  initAllocator — VMA (Vulkan Memory Allocator)
//
//  *instance / *device / *physDevice dereference the raii wrappers to their
//  underlying vk:: types, which implicitly convert to VkXxx for VMA.
// =============================================================================
bool Step01Renderer::initAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice   = *physDevice;
    allocatorInfo.device           = *device;
    allocatorInfo.instance         = *instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

    vk::Result result = static_cast<vk::Result>(vmaCreateAllocator(&allocatorInfo, &allocator));
    if (result != vk::Result::eSuccess) {
        Log::Error("Failed to create VMA allocator (%s).", vk::to_string(result).c_str());
        return false;
    }

    Log::Info("VMA allocator created.");
    return true;
}

// =============================================================================
//  initPipeline — creates the graphics pipeline using dynamic rendering
//
//  No VkRenderPass is needed: vk::PipelineRenderingCreateInfo is chained via
//  pNext (Vulkan 1.3 dynamic rendering, already enabled in initVulkan).
//  Vertices are hardcoded in the shader — vertex input state is empty.
//  Viewport and scissor are dynamic states, set at draw time.
// =============================================================================
bool Step01Renderer::initPipeline() {
    // 1. Load shader (both stages share the same SPIR-V module)
    auto shaderModule = loadShaderModule(device, "assets/shaders/01.BasicPipeline.spv");
    if (!*shaderModule) return false;

    // 2. Shader stages
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {{
        {
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = *shaderModule,
            .pName  = "vertMain",
        },
        {
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = *shaderModule,
            .pName  = "fragMain",
        },
    }};

    // 3. Vertex input — empty, vertices are hardcoded in the shader
    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    // 4. Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology               = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = VK_FALSE,
    };

    // 5. Viewport + scissor as dynamic states (set at draw time)
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data(),
    };
    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    // 6. Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eClockwise,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
    };

    // 7. Multisampling — disabled
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = VK_FALSE,
    };

    // 8. Color blend attachment — blending off, full RGBA write mask
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable    = VK_FALSE,
        .colorWriteMask = vk::ColorComponentFlagBits::eR |
                          vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB |
                          vk::ColorComponentFlagBits::eA,
    };

    // 9. Color blending
    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
    };

    // 10. Pipeline layout (no descriptors, no push constants)
    pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{});

    // 11. Dynamic rendering info — tells the pipeline which attachment formats
    //     to expect without requiring a VkRenderPass object.
    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swapchainFormat,
    };

    // 12–13. Assemble and create the pipeline
    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext               = &renderingInfo,
        .stageCount          = static_cast<uint32_t>(shaderStages.size()),
        .pStages             = shaderStages.data(),
        .pVertexInputState   = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = &dynamicState,
        .layout              = *pipelineLayout,
    };

    graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineInfo);

    // 14. shaderModule destroyed here (RAII, end of scope)
    Log::Info("Graphics pipeline created.");
    return true;
}
