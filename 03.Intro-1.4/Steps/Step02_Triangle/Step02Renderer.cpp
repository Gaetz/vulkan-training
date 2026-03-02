#include "Step02Renderer.hpp"
#include "../../BasicServices/Log.h"

#include <VkBootstrap.h>
#include <SDL3/SDL_vulkan.h>

#include <array>
#include <fstream>

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
// =============================================================================
Step02Renderer::~Step02Renderer() {
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }
}

// =============================================================================
//  IRenderer::init
// =============================================================================
bool Step02Renderer::init(SDL_Window* window) {
    if (!initVulkan(window))       return false;
    if (!initSwapchain(window))    return false;
    if (!initImageViews())         return false;
    if (!initAllocator())          return false;
    if (!initPipeline())           return false;
    if (!initCommandBuffers())     return false;
    if (!initSyncObjects())        return false;

    Log::Info("Step02Renderer initialized — triangle rendering ready.");
    return true;
}

// =============================================================================
//  IRenderer::cleanup
// =============================================================================
void Step02Renderer::cleanup() {
    device.waitIdle();

    inFlightFences.clear();
    renderFinishedSemaphores.clear();
    imageAvailableSemaphores.clear();
    commandBuffers.clear();
    commandPool.clear();

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

    Log::Info("Step02Renderer cleaned up.");
}

// =============================================================================
//  IRenderer::render
// =============================================================================
void Step02Renderer::render(IScene& /*scene*/) {
    // 1. Wait for the GPU to finish with this frame slot
    auto waitResult = device.waitForFences(*inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    if (waitResult != vk::Result::eSuccess) return;

    // 2. Acquire next swapchain image
    auto [acqResult, imageIndex] = swapchain.acquireNextImage(
        UINT64_MAX, *imageAvailableSemaphores[currentFrame]);
    if (acqResult != vk::Result::eSuccess && acqResult != vk::Result::eSuboptimalKHR)
        return;

    // 3. Reset fence and command buffer for this frame
    device.resetFences(*inFlightFences[currentFrame]);
    commandBuffers[currentFrame].reset();

    // 4. Record draw commands
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    // 5. Submit to the graphics queue
    vk::Semaphore          waitSem   = *imageAvailableSemaphores[currentFrame];
    vk::Semaphore          signalSem = *renderFinishedSemaphores[imageIndex]; // indexed by image, not frame slot
    vk::CommandBuffer      cmd       = *commandBuffers[currentFrame];
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    graphicsQueue.submit(vk::SubmitInfo{
        .waitSemaphoreCount   = 1, .pWaitSemaphores   = &waitSem,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1, .pCommandBuffers   = &cmd,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &signalSem,
    }, *inFlightFences[currentFrame]);

    // 6. Present
    vk::SwapchainKHR sc = *swapchain;
    try {
        auto presentResult = presentQueue.presentKHR(vk::PresentInfoKHR{
            .waitSemaphoreCount = 1, .pWaitSemaphores = &signalSem,
            .swapchainCount     = 1, .pSwapchains     = &sc,
            .pImageIndices      = &imageIndex,
        });
        (void)presentResult; // eSuccess or eSuboptimalKHR — resize handled in a later step
    } catch (vk::OutOfDateKHRError&) {
        // Swapchain out of date (e.g. window resized) — handled in a later step
    }

    // 7. Advance to next frame slot
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// =============================================================================
//  recordCommandBuffer — records one frame's draw commands
// =============================================================================
void Step02Renderer::recordCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex) {
    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    // --- Transition: UNDEFINED → eColorAttachmentOptimal ---
    vk::ImageMemoryBarrier2 toAttachment{
        .srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask = vk::AccessFlagBits2::eNone,
        .dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout     = vk::ImageLayout::eUndefined,
        .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
        .image         = swapchainImages[imageIndex],
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &toAttachment,
    });

    // --- Begin dynamic rendering ---
    vk::RenderingAttachmentInfo colorAttachment{
        .imageView   = *imageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = vk::ClearValue{vk::ClearColorValue{
            std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    cmd.beginRendering(vk::RenderingInfo{
        .renderArea           = vk::Rect2D{{0, 0}, swapchainExtent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
    });

    // --- Draw ---
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    vk::Viewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(swapchainExtent.width),
        .height   = static_cast<float>(swapchainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vk::Rect2D scissor{.offset = {0, 0}, .extent = swapchainExtent};
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    cmd.draw(3, 1, 0, 0);

    cmd.endRendering();

    // --- Transition: eColorAttachmentOptimal → ePresentSrcKHR ---
    vk::ImageMemoryBarrier2 toPresent{
        .srcStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask  = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask = vk::AccessFlagBits2::eNone,
        .oldLayout     = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout     = vk::ImageLayout::ePresentSrcKHR,
        .image         = swapchainImages[imageIndex],
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &toPresent,
    });

    cmd.end();
}

// =============================================================================
//  initCommandBuffers — command pool + one buffer per frame in flight
// =============================================================================
bool Step02Renderer::initCommandBuffers() {
    commandPool = device.createCommandPool(vk::CommandPoolCreateInfo{
        .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphicsQueueFamily,
    });

    commandBuffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        .commandPool        = *commandPool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    });

    Log::Info("Command pool and %u command buffers created.", MAX_FRAMES_IN_FLIGHT);
    return true;
}

// =============================================================================
//  initSyncObjects
//
//  imageAvailableSemaphores + inFlightFences: one per frame slot.
//  renderFinishedSemaphores: one per SWAPCHAIN IMAGE (not per frame slot).
//
//  Why per-image for renderFinished?
//  waitForFences() only guarantees that rendering is done, not that the
//  presentation engine has consumed the semaphore it was waiting on.
//  The spec guarantees: when acquireNextImage returns imageIndex N, the
//  previous presentation of image N is complete — which includes consuming
//  renderFinishedSemaphores[N]. So indexing by image makes reuse safe.
// =============================================================================
bool Step02Renderer::initSyncObjects() {
    // Per-frame-slot: imageAvailable + fence
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imageAvailableSemaphores.push_back(device.createSemaphore({}));
        inFlightFences.push_back(device.createFence(vk::FenceCreateInfo{
            // Pre-signaled so the first frame doesn't wait forever
            .flags = vk::FenceCreateFlagBits::eSignaled,
        }));
    }

    // Per-swapchain-image: renderFinished
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        renderFinishedSemaphores.push_back(device.createSemaphore({}));
    }

    Log::Info("Sync objects created (%u frame slots, %zu render-finished semaphores).",
              MAX_FRAMES_IN_FLIGHT, swapchainImages.size());
    return true;
}

// =============================================================================
//  initVulkan — instance, surface, physical device, logical device, queues
// =============================================================================
bool Step02Renderer::initVulkan(SDL_Window* window) {

    auto instanceResult = vkb::InstanceBuilder{}
        .set_app_name("Vulkan Tutorial — Step 02")
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
    if (vkbInst.debug_messenger != VK_NULL_HANDLE) {
        debugMessenger = vk::raii::DebugUtilsMessengerEXT{instance,
                                                          vkbInst.debug_messenger};
    }
#endif

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window,
                                  static_cast<VkInstance>(*instance),
                                  nullptr, &rawSurface)) {
        Log::Error("Failed to create Vulkan surface: %s", SDL_GetError());
        return false;
    }
    surface = vk::raii::SurfaceKHR{instance, rawSurface};
    Log::Info("Window surface created.");

    auto physDeviceResult = vkb::PhysicalDeviceSelector{vkbInst}
        .set_surface(rawSurface)
        .set_minimum_version(1, 4)
        // slangc emits BaseVertex (DrawParameters capability) when translating
        // SV_VertexID to match HLSL semantics (VertexIndex - BaseVertex).
        // shaderDrawParameters is mandatory on all Vulkan 1.1+ implementations.
        .set_required_features_11(VkPhysicalDeviceVulkan11Features{
            .shaderDrawParameters = VK_TRUE,
        })
        .set_required_features_13(VkPhysicalDeviceVulkan13Features{
            .synchronization2  = VK_TRUE,
            .dynamicRendering  = VK_TRUE,
        })
        .select();

    if (!physDeviceResult) {
        Log::Error("Failed to select physical device: %s",
                   physDeviceResult.error().message().c_str());
        return false;
    }

    auto vkbPhysDev = physDeviceResult.value();
    physDevice = vk::raii::PhysicalDevice{instance, vkbPhysDev.physical_device};
    Log::Info("Physical device: %s", physDevice.getProperties().deviceName.data());

    auto deviceResult = vkb::DeviceBuilder{vkbPhysDev}.build();

    if (!deviceResult) {
        Log::Error("Failed to create logical device: %s",
                   deviceResult.error().message().c_str());
        return false;
    }

    auto vkbDev = deviceResult.value();
    device = vk::raii::Device{physDevice, vkbDev.device};
    Log::Info("Logical device created.");

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
//  initSwapchain
// =============================================================================
bool Step02Renderer::initSwapchain(SDL_Window* window) {
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
    swapchainImages = swapchain.getImages();

    Log::Info("Swapchain created — %ux%u, format %s, %zu images.",
              swapchainExtent.width, swapchainExtent.height,
              vk::to_string(swapchainFormat).c_str(),
              swapchainImages.size());
    return true;
}

// =============================================================================
//  initImageViews
// =============================================================================
bool Step02Renderer::initImageViews() {
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
//  initAllocator — VMA
// =============================================================================
bool Step02Renderer::initAllocator() {
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
//  loadShaderModule
// =============================================================================
vk::raii::ShaderModule Step02Renderer::loadShaderModule(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        Log::Error("Failed to open shader file: %s", path.c_str());
        return vk::raii::ShaderModule{nullptr};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));

    return device.createShaderModule(vk::ShaderModuleCreateInfo{
        .codeSize = fileSize,
        .pCode    = code.data(),
    });
}

// =============================================================================
//  initPipeline — same pipeline as Step01, reuses 01.BasicPipeline.spv
// =============================================================================
bool Step02Renderer::initPipeline() {
    auto shaderModule = loadShaderModule("assets/shaders/01.BasicPipeline.spv");
    if (!*shaderModule) return false;

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

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology               = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = VK_FALSE,
    };

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

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eClockwise,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = VK_FALSE,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable    = VK_FALSE,
        .colorWriteMask = vk::ColorComponentFlagBits::eR |
                          vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB |
                          vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
    };

    pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{});

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swapchainFormat,
    };

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

    Log::Info("Graphics pipeline created.");
    return true;
}
