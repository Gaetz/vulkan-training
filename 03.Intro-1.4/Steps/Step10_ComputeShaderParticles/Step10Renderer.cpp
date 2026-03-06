#include "Step10Renderer.hpp"
#include "Step10Scene.hpp"
#include "../../BasicServices/Log.h"
#include "../../Engine/Renderer/ImmediateSubmit.hpp"

#include <VkBootstrap.h>
#include <SDL3/SDL_vulkan.h>

#include <array>
#include <cstring>

using services::Log;

// =============================================================================
//  Debug messenger callback
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
//  destroyVMAResources
// =============================================================================
void Step10Renderer::destroyVMAResources() {
    for (auto& b : particleBuffers)  b.destroy();
    particleBuffers.clear();
    for (auto& b : deltaTimeBuffers) b.destroy();
    deltaTimeBuffers.clear();
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }
}

// =============================================================================
//  Destructor
// =============================================================================
Step10Renderer::~Step10Renderer() {
    destroyVMAResources();
}

// =============================================================================
//  init
// =============================================================================
bool Step10Renderer::init(SDL_Window* w) {
    window = w;

    if (!initVulkan(window))       return false;
    if (!initSwapchain(window))    return false;
    if (!initImageViews())         return false;
    if (!initAllocator())          return false;
    if (!initPipelines())          return false;
    if (!initCommandBuffers())     return false;
    if (!initSyncObjects())        return false;

    Log::Info("Step10Renderer initialized — %u particles.", PARTICLE_COUNT);
    return true;
}

// =============================================================================
//  onSceneReady — called by Engine after scene->init()
// =============================================================================
void Step10Renderer::onSceneReady(IScene& scene) {
    auto& s = static_cast<Step10Scene&>(scene);

    // Convert Step10Scene::Particle to Step10Renderer::Particle (same layout)
    const auto& srcParticles = s.getParticles();
    std::vector<Particle> gpuParticles(srcParticles.size());
    static_assert(sizeof(Step10Scene::Particle) == sizeof(Particle),
                  "Particle layout mismatch between scene and renderer");
    std::memcpy(gpuParticles.data(), srcParticles.data(),
                srcParticles.size() * sizeof(Particle));

    initParticleBuffers(gpuParticles);
    initDescriptors();
}

// =============================================================================
//  cleanup
// =============================================================================
void Step10Renderer::cleanup() {
    device.waitIdle();

    computeInFlightFences.clear();
    graphicsInFlightFences.clear();
    computeFinishedSemaphores.clear();
    renderFinishedSemaphores.clear();
    imageAvailableSemaphores.clear();

    graphicsCommandBuffers.clear();
    computeCommandBuffers.clear();
    commandPool.clear();

    computeDescriptorSets.clear();
    descriptorPool.clear();

    computePipeline.clear();
    computePipelineLayout.clear();
    computeDescriptorSetLayout.clear();

    graphicsPipeline.clear();
    graphicsPipelineLayout.clear();

    cleanupSwapchain();
    destroyVMAResources();

    device.clear();
    surface.clear();
    debugMessenger.clear();
    instance.clear();

    Log::Info("Step10Renderer cleaned up.");
}

// =============================================================================
//  cleanupSwapchain / recreateSwapchain
// =============================================================================
void Step10Renderer::cleanupSwapchain() {
    imageViews.clear();
    swapchain.clear();
}

void Step10Renderer::recreateSwapchain() {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    while (w == 0 || h == 0) {
        SDL_WaitEvent(nullptr);
        SDL_GetWindowSizeInPixels(window, &w, &h);
    }

    device.waitIdle();
    cleanupSwapchain();
    initSwapchain(window);
    initImageViews();

    if (renderFinishedSemaphores.size() != swapchainImages.size()) {
        renderFinishedSemaphores.clear();
        for (size_t i = 0; i < swapchainImages.size(); ++i)
            renderFinishedSemaphores.push_back(device.createSemaphore({}));
    }

    Log::Info("Swapchain recreated — %ux%u.", swapchainExtent.width, swapchainExtent.height);
}

// =============================================================================
//  initParticleBuffers
// =============================================================================
bool Step10Renderer::initParticleBuffers(const std::vector<Particle>& particles) {
    const vk::DeviceSize bufferSize = sizeof(Particle) * particles.size();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // Staging buffer (host-visible)
        Buffer staging{
            allocator,
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        };
        staging.upload(particles.data(), static_cast<size_t>(bufferSize));

        // Device-local SSBO + vertex buffer
        particleBuffers.emplace_back(
            allocator,
            bufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer  |
            vk::BufferUsageFlagBits::eVertexBuffer   |
            vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            0 // no HOST_ACCESS flags
        );

        // Copy staging → device-local
        ImmediateSubmit{device, commandPool, graphicsQueue}([&](vk::CommandBuffer cmd) {
            vk::BufferCopy region{.size = bufferSize};
            cmd.copyBuffer(staging.get(), particleBuffers[i].get(), region);
        });

        staging.destroy();

        // Delta-time UBO (persistently mapped)
        deltaTimeBuffers.emplace_back(
            allocator,
            sizeof(DeltaTimeUBO),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
    }

    Log::Info("Particle buffers created — %u SSBOs, %u UBOs.", MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);
    return true;
}

// =============================================================================
//  initDescriptors
// =============================================================================
bool Step10Renderer::initDescriptors() {
    constexpr uint32_t N = MAX_FRAMES_IN_FLIGHT;

    std::array<vk::DescriptorPoolSize, 2> poolSizes = {{
        {.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = N},
        {.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = N * 2},
    }};
    descriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
        .maxSets       = N,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data(),
    });

    std::vector<vk::DescriptorSetLayout> layouts(N, *computeDescriptorSetLayout);
    computeDescriptorSets = (*device).allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        .descriptorPool     = *descriptorPool,
        .descriptorSetCount = N,
        .pSetLayouts        = layouts.data(),
    });

    for (uint32_t i = 0; i < N; ++i) {
        // binding 0 — deltaTime UBO
        vk::DescriptorBufferInfo uboInfo{
            .buffer = deltaTimeBuffers[i].get(),
            .offset = 0,
            .range  = sizeof(DeltaTimeUBO),
        };
        // binding 1 — previous frame SSBO (read-only input)
        uint32_t prevIdx = (i + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
        vk::DescriptorBufferInfo prevInfo{
            .buffer = particleBuffers[prevIdx].get(),
            .offset = 0,
            .range  = sizeof(Particle) * PARTICLE_COUNT,
        };
        // binding 2 — current frame SSBO (written by compute, read by graphics)
        vk::DescriptorBufferInfo currInfo{
            .buffer = particleBuffers[i].get(),
            .offset = 0,
            .range  = sizeof(Particle) * PARTICLE_COUNT,
        };

        std::array<vk::WriteDescriptorSet, 3> writes = {{
            {
                .dstSet = computeDescriptorSets[i], .dstBinding = 0,
                .dstArrayElement = 0, .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo     = &uboInfo,
            },
            {
                .dstSet = computeDescriptorSets[i], .dstBinding = 1,
                .dstArrayElement = 0, .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo     = &prevInfo,
            },
            {
                .dstSet = computeDescriptorSets[i], .dstBinding = 2,
                .dstArrayElement = 0, .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo     = &currInfo,
            },
        }};
        (*device).updateDescriptorSets(writes, nullptr);
    }

    Log::Info("Compute descriptor sets created — %u sets (UBO + 2 SSBOs each).", N);
    return true;
}

// =============================================================================
//  render
// =============================================================================
void Step10Renderer::render(IScene& scene) {
    auto& s = static_cast<Step10Scene&>(scene);

    // ── Compute pass ─────────────────────────────────────────────────────────
    auto computeWait = device.waitForFences(*computeInFlightFences[currentFrame], true, UINT64_MAX);
    if (computeWait != vk::Result::eSuccess) return;

    // Upload delta time
    DeltaTimeUBO ubo{.deltaTime = s.getDeltaTime()};
    deltaTimeBuffers[currentFrame].upload(&ubo, sizeof(ubo));

    device.resetFences(*computeInFlightFences[currentFrame]);
    computeCommandBuffers[currentFrame].reset();
    recordComputeCommandBuffer(computeCommandBuffers[currentFrame]);

    vk::CommandBuffer computeCmd = *computeCommandBuffers[currentFrame];
    graphicsQueue.submit(vk::SubmitInfo{
        .commandBufferCount   = 1, .pCommandBuffers   = &computeCmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &*computeFinishedSemaphores[currentFrame],
    }, *computeInFlightFences[currentFrame]);

    // ── Graphics pass ─────────────────────────────────────────────────────────
    auto gfxWait = device.waitForFences(*graphicsInFlightFences[currentFrame], true, UINT64_MAX);
    if (gfxWait != vk::Result::eSuccess) return;

    uint32_t imageIndex = 0;
    {
        vk::Result acqResult;
        try {
            auto [r, i] = swapchain.acquireNextImage(
                UINT64_MAX, *imageAvailableSemaphores[currentFrame]);
            acqResult  = r;
            imageIndex = i;
        } catch (vk::OutOfDateKHRError&) {
            recreateSwapchain();
            return;
        }
        if (acqResult != vk::Result::eSuccess && acqResult != vk::Result::eSuboptimalKHR)
            return;
    }

    device.resetFences(*graphicsInFlightFences[currentFrame]);
    graphicsCommandBuffers[currentFrame].reset();
    recordGraphicsCommandBuffer(graphicsCommandBuffers[currentFrame], imageIndex);

    std::array<vk::Semaphore, 2> waitSems = {
        *imageAvailableSemaphores[currentFrame],
        *computeFinishedSemaphores[currentFrame],
    };
    std::array<vk::PipelineStageFlags, 2> waitStages = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eVertexInput,
    };
    vk::CommandBuffer gfxCmd = *graphicsCommandBuffers[currentFrame];
    vk::Semaphore     signalSem = *renderFinishedSemaphores[imageIndex];

    graphicsQueue.submit(vk::SubmitInfo{
        .waitSemaphoreCount   = static_cast<uint32_t>(waitSems.size()),
        .pWaitSemaphores      = waitSems.data(),
        .pWaitDstStageMask    = waitStages.data(),
        .commandBufferCount   = 1, .pCommandBuffers   = &gfxCmd,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &signalSem,
    }, *graphicsInFlightFences[currentFrame]);

    vk::SwapchainKHR sc = *swapchain;
    try {
        auto presentResult = presentQueue.presentKHR(vk::PresentInfoKHR{
            .waitSemaphoreCount = 1, .pWaitSemaphores = &signalSem,
            .swapchainCount     = 1, .pSwapchains     = &sc,
            .pImageIndices      = &imageIndex,
        });
        if (presentResult == vk::Result::eSuboptimalKHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapchain();
        }
    } catch (vk::OutOfDateKHRError&) {
        framebufferResized = false;
        recreateSwapchain();
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// =============================================================================
//  recordComputeCommandBuffer
// =============================================================================
void Step10Renderer::recordComputeCommandBuffer(vk::raii::CommandBuffer& cmd) {
    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePipelineLayout,
                            0, computeDescriptorSets[currentFrame], {});
    cmd.dispatch(PARTICLE_COUNT / 256, 1, 1);

    cmd.end();
}

// =============================================================================
//  recordGraphicsCommandBuffer
// =============================================================================
void Step10Renderer::recordGraphicsCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex) {
    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    // Transition swapchain image: eUndefined → eColorAttachmentOptimal
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
            .baseMipLevel   = 0, .levelCount     = 1,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &toAttachment,
    });

    // Begin rendering — color only, black clear
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

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    vk::Viewport viewport{
        .x = 0, .y = 0,
        .width    = static_cast<float>(swapchainExtent.width),
        .height   = static_cast<float>(swapchainExtent.height),
        .minDepth = 0.0f, .maxDepth = 1.0f,
    };
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, vk::Rect2D{{0, 0}, swapchainExtent});

    cmd.bindVertexBuffers(0, {particleBuffers[currentFrame].get()}, {vk::DeviceSize{0}});
    cmd.draw(PARTICLE_COUNT, 1, 0, 0);

    cmd.endRendering();

    // Transition swapchain image: eColorAttachmentOptimal → ePresentSrcKHR
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
            .baseMipLevel   = 0, .levelCount     = 1,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &toPresent,
    });

    cmd.end();
}

// =============================================================================
//  initCommandBuffers
// =============================================================================
bool Step10Renderer::initCommandBuffers() {
    commandPool = device.createCommandPool(vk::CommandPoolCreateInfo{
        .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphicsQueueFamily,
    });

    computeCommandBuffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        .commandPool        = *commandPool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    });
    graphicsCommandBuffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        .commandPool        = *commandPool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    });

    Log::Info("Command pool and %u compute + %u graphics command buffers created.",
              MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);
    return true;
}

// =============================================================================
//  initSyncObjects
// =============================================================================
bool Step10Renderer::initSyncObjects() {
    vk::FenceCreateInfo signaledFence{.flags = vk::FenceCreateFlagBits::eSignaled};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imageAvailableSemaphores.push_back(device.createSemaphore({}));
        computeFinishedSemaphores.push_back(device.createSemaphore({}));
        graphicsInFlightFences.push_back(device.createFence(signaledFence));
        computeInFlightFences.push_back(device.createFence(signaledFence));
    }
    for (size_t i = 0; i < swapchainImages.size(); ++i)
        renderFinishedSemaphores.push_back(device.createSemaphore({}));

    Log::Info("Sync objects created (%u frame slots, %zu render-finished semaphores).",
              MAX_FRAMES_IN_FLIGHT, swapchainImages.size());
    return true;
}

// =============================================================================
//  initVulkan
// =============================================================================
bool Step10Renderer::initVulkan(SDL_Window* w) {
    auto instanceResult = vkb::InstanceBuilder{}
        .set_app_name("Vulkan Tutorial — Step 10")
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
    if (vkbInst.debug_messenger != VK_NULL_HANDLE)
        debugMessenger = vk::raii::DebugUtilsMessengerEXT{instance, vkbInst.debug_messenger};
#endif

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(w, static_cast<VkInstance>(*instance), nullptr, &rawSurface)) {
        Log::Error("Failed to create Vulkan surface: %s", SDL_GetError());
        return false;
    }
    surface = vk::raii::SurfaceKHR{instance, rawSurface};
    Log::Info("Window surface created.");

    auto physDeviceResult = vkb::PhysicalDeviceSelector{vkbInst}
        .set_surface(rawSurface)
        .set_minimum_version(1, 4)
        .set_required_features_13(VkPhysicalDeviceVulkan13Features{
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE,
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

    Log::Info("Queues — graphics %u, present %u.", graphicsQueueFamily, presentQueueFamily);
    return true;
}

// =============================================================================
//  initSwapchain
// =============================================================================
bool Step10Renderer::initSwapchain(SDL_Window* w) {
    int fbWidth = 0, fbHeight = 0;
    SDL_GetWindowSizeInPixels(w, &fbWidth, &fbHeight);

    auto swapResult = vkb::SwapchainBuilder{
            static_cast<VkPhysicalDevice>(*physDevice),
            static_cast<VkDevice>(*device),
            static_cast<VkSurfaceKHR>(*surface),
            graphicsQueueFamily, presentQueueFamily}
        .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eMailbox))
        .set_desired_format({static_cast<VkFormat>(vk::Format::eB8G8R8A8Srgb),
                              static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear)})
        .set_desired_extent(static_cast<uint32_t>(fbWidth),
                            static_cast<uint32_t>(fbHeight))
        .set_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eColorAttachment))
        .build();

    if (!swapResult) {
        Log::Error("Failed to create swapchain: %s", swapResult.error().message().c_str());
        return false;
    }

    auto vkbSwap    = swapResult.value();
    swapchain       = vk::raii::SwapchainKHR{device, vkbSwap.swapchain};
    swapchainFormat = static_cast<vk::Format>(vkbSwap.image_format);
    swapchainExtent = vk::Extent2D{vkbSwap.extent.width, vkbSwap.extent.height};
    swapchainImages = swapchain.getImages();

    Log::Info("Swapchain created — %ux%u, format %s, %zu images.",
              swapchainExtent.width, swapchainExtent.height,
              vk::to_string(swapchainFormat).c_str(), swapchainImages.size());
    return true;
}

// =============================================================================
//  initImageViews
// =============================================================================
bool Step10Renderer::initImageViews() {
    for (vk::Image image : swapchainImages) {
        imageViews.push_back(device.createImageView(vk::ImageViewCreateInfo{
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
                .baseMipLevel   = 0, .levelCount     = 1,
                .baseArrayLayer = 0, .layerCount     = 1,
            },
        }));
    }
    Log::Info("%zu image views created.", imageViews.size());
    return true;
}

// =============================================================================
//  initAllocator
// =============================================================================
bool Step10Renderer::initAllocator() {
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
//  initPipelines — compute descriptor layout, compute pipeline, graphics pipeline
// =============================================================================
bool Step10Renderer::initPipelines() {
    // ── Compute descriptor set layout ────────────────────────────────────────
    std::array<vk::DescriptorSetLayoutBinding, 3> computeBindings = {{
        {
            .binding         = 0,
            .descriptorType  = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eCompute,
        },
        {
            .binding         = 1,
            .descriptorType  = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eCompute,
        },
        {
            .binding         = 2,
            .descriptorType  = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eCompute,
        },
    }};
    computeDescriptorSetLayout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(computeBindings.size()),
        .pBindings    = computeBindings.data(),
    });

    computePipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts    = &*computeDescriptorSetLayout,
    });

    // ── Compute pipeline ─────────────────────────────────────────────────────
    auto computeModule = loadShaderModule(device, "assets/shaders/10.ComputeParticles.compute.spv");
    if (!*computeModule) return false;

    computePipeline = device.createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
        .stage  = vk::PipelineShaderStageCreateInfo{
            .stage  = vk::ShaderStageFlagBits::eCompute,
            .module = *computeModule,
            .pName  = "main",  // slangc renames single-entry compilations to "main" in SPIR-V
        },
        .layout = *computePipelineLayout,
    });

    Log::Info("Compute pipeline created.");

    // ── Graphics pipeline ─────────────────────────────────────────────────────
    auto graphicsModule = loadShaderModule(device, "assets/shaders/10.ComputeParticles.graphics.spv");
    if (!*graphicsModule) return false;

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {{
        {.stage = vk::ShaderStageFlagBits::eVertex,   .module = *graphicsModule, .pName = "vertMain"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = *graphicsModule, .pName = "fragMain"},
    }};

    auto bindingDesc   = Particle::getBindingDescription();
    auto attributeDesc = Particle::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInput{
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDesc,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size()),
        .pVertexAttributeDescriptions    = attributeDesc.data(),
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology               = vk::PrimitiveTopology::ePointList,
        .primitiveRestartEnable = false,
    };

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data(),
    };
    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable        = false,
        .rasterizerDiscardEnable = false,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eNone,
        .frontFace               = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable         = false,
        .lineWidth               = 1.0f,
    };
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = false,
    };

    // Alpha blending for particles
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable           = vk::True,
        .srcColorBlendFactor   = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor   = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp          = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor   = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor   = vk::BlendFactor::eZero,
        .alphaBlendOp          = vk::BlendOp::eAdd,
        .colorWriteMask        = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                 vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable   = false,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
    };

    // Empty layout — no descriptor sets on graphics side
    graphicsPipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{});

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swapchainFormat,
        // no depth attachment
    };
    graphicsPipeline = device.createGraphicsPipeline(nullptr, vk::GraphicsPipelineCreateInfo{
        .pNext               = &renderingInfo,
        .stageCount          = static_cast<uint32_t>(shaderStages.size()),
        .pStages             = shaderStages.data(),
        .pVertexInputState   = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = nullptr,  // no depth test for 2D particles
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = &dynamicState,
        .layout              = *graphicsPipelineLayout,
    });

    Log::Info("Graphics pipeline created (point sprites, alpha blending).");
    return true;
}
