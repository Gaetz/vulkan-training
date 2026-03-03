#include "Step08Renderer.hpp"
#include "Step08Scene.hpp"
#include "../../BasicServices/Log.h"
#include "../../Engine/Renderer/TextureLoader.hpp"

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
void Step08Renderer::destroyVMAResources() {
    for (auto& ub : uniformBuffers) ub.destroy();
    uniformBuffers.clear();
    texture.destroy();
    depthImage.destroy();
    model.destroy();
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }
}

// =============================================================================
//  Destructor
// =============================================================================
Step08Renderer::~Step08Renderer() {
    destroyVMAResources();
}

// =============================================================================
//  init
// =============================================================================
bool Step08Renderer::init(SDL_Window* w) {
    window = w;

    if (!initVulkan(window))       return false;
    if (!initSwapchain(window))    return false;
    if (!initImageViews())         return false;
    if (!initAllocator())          return false;

    depthFormat = findDepthFormat();
    if (depthFormat == vk::Format::eUndefined) return false;

    if (!initPipeline())           return false;
    if (!initCommandBuffers())     return false;
    if (!initSyncObjects())        return false;
    if (!initModel())              return false;
    if (!initDepthResources())     return false;
    if (!initTexture())            return false;
    if (!initDescriptors())        return false;

    Log::Info("Step08Renderer initialized — 3D model loaded.");
    return true;
}

// =============================================================================
//  cleanup
// =============================================================================
void Step08Renderer::cleanup() {
    device.waitIdle();

    inFlightFences.clear();
    renderFinishedSemaphores.clear();
    imageAvailableSemaphores.clear();
    commandBuffers.clear();
    commandPool.clear();

    graphicsPipeline.clear();
    pipelineLayout.clear();

    descriptorSets.clear();
    descriptorPool.clear();
    descriptorSetLayout.clear();

    cleanupSwapchain();

    destroyVMAResources();

    device.clear();
    surface.clear();
    debugMessenger.clear();
    instance.clear();

    Log::Info("Step08Renderer cleaned up.");
}

// =============================================================================
//  cleanupSwapchain / recreateSwapchain
// =============================================================================
void Step08Renderer::cleanupSwapchain() {
    depthImageView.clear();
    depthImage.destroy();
    imageViews.clear();
    swapchain.clear();
}

void Step08Renderer::recreateSwapchain() {
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
    initDepthResources();

    if (renderFinishedSemaphores.size() != swapchainImages.size()) {
        renderFinishedSemaphores.clear();
        for (size_t i = 0; i < swapchainImages.size(); ++i)
            renderFinishedSemaphores.push_back(device.createSemaphore({}));
    }

    Log::Info("Swapchain recreated — %ux%u.", swapchainExtent.width, swapchainExtent.height);
}

// =============================================================================
//  findDepthFormat
// =============================================================================
vk::Format Step08Renderer::findDepthFormat() {
    for (vk::Format fmt : {
            vk::Format::eD32Sfloat,
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
    }) {
        auto props = physDevice.getFormatProperties(fmt);
        if ((props.optimalTilingFeatures &
             vk::FormatFeatureFlagBits::eDepthStencilAttachment) ==
             vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            Log::Info("Depth format selected: %s.", vk::to_string(fmt).c_str());
            return fmt;
        }
    }
    Log::Error("No supported depth format found.");
    return vk::Format::eUndefined;
}

// =============================================================================
//  initDepthResources
// =============================================================================
bool Step08Renderer::initDepthResources() {
    depthImage = Image{allocator,
        swapchainExtent.width,
        swapchainExtent.height,
        depthFormat,
        vk::ImageUsageFlagBits::eDepthStencilAttachment};

    depthImageView = device.createImageView(vk::ImageViewCreateInfo{
        .image    = depthImage.get(),
        .viewType = vk::ImageViewType::e2D,
        .format   = depthFormat,
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eDepth,
            .baseMipLevel   = 0, .levelCount     = 1,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    });

    Log::Info("Depth resources created (%ux%u, %s).",
              swapchainExtent.width, swapchainExtent.height,
              vk::to_string(depthFormat).c_str());
    return true;
}

// =============================================================================
//  initModel — delegate OBJ loading + GPU upload to ModelLoader.
// =============================================================================
bool Step08Renderer::initModel() {
    model = ModelLoader{device, allocator, commandPool, graphicsQueue}
                .load("assets/models/viking_room.obj");
    return model.valid();
}

// =============================================================================
//  initTexture
// =============================================================================
bool Step08Renderer::initTexture() {
    texture = TextureLoader{device, physDevice, allocator, commandPool, graphicsQueue}
                  .load("assets/textures/viking_room.png");
    return texture.getImage() != vk::Image{nullptr};
}

// =============================================================================
//  initDescriptors
// =============================================================================
bool Step08Renderer::initDescriptors() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        uniformBuffers.emplace_back(
            allocator,
            sizeof(UniformBufferObject),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    std::array<vk::DescriptorPoolSize, 2> poolSizes = {{
        {.type = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = MAX_FRAMES_IN_FLIGHT},
        {.type = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = MAX_FRAMES_IN_FLIGHT},
    }};
    descriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
        .maxSets       = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data(),
    });

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
    descriptorSets = (*device).allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        .descriptorPool     = *descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts        = layouts.data(),
    });

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffers[i].get(),
            .offset = 0,
            .range  = sizeof(UniformBufferObject),
        };
        vk::DescriptorImageInfo imageInfo{
            .sampler     = texture.getSampler(),
            .imageView   = texture.getView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        std::array<vk::WriteDescriptorSet, 2> writes = {{
            {
                .dstSet = descriptorSets[i], .dstBinding = 0,
                .dstArrayElement = 0, .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo     = &bufferInfo,
            },
            {
                .dstSet = descriptorSets[i], .dstBinding = 1,
                .dstArrayElement = 0, .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo      = &imageInfo,
            },
        }};
        (*device).updateDescriptorSets(writes, nullptr);
    }

    Log::Info("Descriptors ready — %u UBOs, %u combined image samplers.",
              MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);
    return true;
}

// =============================================================================
//  render
// =============================================================================
void Step08Renderer::render(IScene& scene) {
    auto waitResult = device.waitForFences(*inFlightFences[currentFrame], true, UINT64_MAX);
    if (waitResult != vk::Result::eSuccess) return;

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

    {
        auto& s = static_cast<Step08Scene&>(scene);
        UniformBufferObject ubo{};
        ubo.model = s.getModelMatrix();
        ubo.view  = s.getViewMatrix();
        ubo.proj  = glm::perspective(glm::radians(45.0f),
                                      static_cast<float>(swapchainExtent.width) /
                                      static_cast<float>(swapchainExtent.height),
                                      0.1f, 10.0f);
        ubo.proj[1][1] *= -1;
        uniformBuffers[currentFrame].upload(&ubo, sizeof(ubo));
    }

    device.resetFences(*inFlightFences[currentFrame]);
    commandBuffers[currentFrame].reset();
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    vk::Semaphore          waitSem   = *imageAvailableSemaphores[currentFrame];
    vk::Semaphore          signalSem = *renderFinishedSemaphores[imageIndex];
    vk::CommandBuffer      cmd       = *commandBuffers[currentFrame];
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    graphicsQueue.submit(vk::SubmitInfo{
        .waitSemaphoreCount   = 1, .pWaitSemaphores   = &waitSem,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1, .pCommandBuffers   = &cmd,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &signalSem,
    }, *inFlightFences[currentFrame]);

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
//  recordCommandBuffer
// =============================================================================
void Step08Renderer::recordCommandBuffer(vk::raii::CommandBuffer& cmd, uint32_t imageIndex) {
    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

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
    vk::ImageMemoryBarrier2 toDepth{
        .srcStageMask        = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                               vk::PipelineStageFlagBits2::eLateFragmentTests,
        .srcAccessMask       = vk::AccessFlagBits2::eNone,
        .dstStageMask        = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                               vk::PipelineStageFlagBits2::eLateFragmentTests,
        .dstAccessMask       = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .oldLayout           = vk::ImageLayout::eUndefined,
        .newLayout           = vk::ImageLayout::eDepthAttachmentOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image               = depthImage.get(),
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eDepth,
            .baseMipLevel   = 0, .levelCount     = 1,
            .baseArrayLayer = 0, .layerCount     = 1,
        },
    };
    std::array<vk::ImageMemoryBarrier2, 2> barriers = {toAttachment, toDepth};
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers    = barriers.data(),
    });

    vk::RenderingAttachmentInfo colorAttachment{
        .imageView   = *imageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = vk::ClearValue{vk::ClearColorValue{
            std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    vk::RenderingAttachmentInfo depthAttachment{
        .imageView   = *depthImageView,
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eDontCare,
        .clearValue  = vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}},
    };
    cmd.beginRendering(vk::RenderingInfo{
        .renderArea           = vk::Rect2D{{0, 0}, swapchainExtent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
        .pDepthAttachment     = &depthAttachment,
    });

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout,
                            0, descriptorSets[currentFrame], {});
    cmd.bindVertexBuffers(0, {model.getVertexBuffer()}, {vk::DeviceSize{0}});
    cmd.bindIndexBuffer(model.getIndexBuffer(), 0, vk::IndexType::eUint32);  // uint32!

    vk::Viewport viewport{
        .x = 0, .y = 0,
        .width    = static_cast<float>(swapchainExtent.width),
        .height   = static_cast<float>(swapchainExtent.height),
        .minDepth = 0.0f, .maxDepth = 1.0f,
    };
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, vk::Rect2D{{0, 0}, swapchainExtent});

    cmd.drawIndexed(model.getIndexCount(), 1, 0, 0, 0);

    cmd.endRendering();

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
bool Step08Renderer::initCommandBuffers() {
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
// =============================================================================
bool Step08Renderer::initSyncObjects() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imageAvailableSemaphores.push_back(device.createSemaphore({}));
        inFlightFences.push_back(device.createFence(vk::FenceCreateInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled,
        }));
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
bool Step08Renderer::initVulkan(SDL_Window* w) {
    auto instanceResult = vkb::InstanceBuilder{}
        .set_app_name("Vulkan Tutorial — Step 08")
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
        .set_required_features(VkPhysicalDeviceFeatures{
            .samplerAnisotropy = VK_TRUE,
        })
        .set_required_features_11(VkPhysicalDeviceVulkan11Features{
            .shaderDrawParameters = VK_TRUE,
        })
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
bool Step08Renderer::initSwapchain(SDL_Window* w) {
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
    swapchainExtent = {vkbSwap.extent.width, vkbSwap.extent.height};
    swapchainImages = swapchain.getImages();

    Log::Info("Swapchain created — %ux%u, format %s, %zu images.",
              swapchainExtent.width, swapchainExtent.height,
              vk::to_string(swapchainFormat).c_str(), swapchainImages.size());
    return true;
}

// =============================================================================
//  initImageViews
// =============================================================================
bool Step08Renderer::initImageViews() {
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
bool Step08Renderer::initAllocator() {
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
//  initPipeline
// =============================================================================
bool Step08Renderer::initPipeline() {
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {{
        {
            .binding         = 0,
            .descriptorType  = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eVertex,
        },
        {
            .binding         = 1,
            .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eFragment,
        },
    }};
    descriptorSetLayout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data(),
    });

    auto shaderModule = loadShaderModule(device, "assets/shaders/08.Modele3D.spv");
    if (!*shaderModule) return false;

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {{
        {.stage = vk::ShaderStageFlagBits::eVertex,   .module = *shaderModule, .pName = "vertMain"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = *shaderModule, .pName = "fragMain"},
    }};

    auto bindingDesc   = Vertex::getBindingDescription();
    auto attributeDesc = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInput{
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDesc,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size()),
        .pVertexAttributeDescriptions    = attributeDesc.data(),
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology               = vk::PrimitiveTopology::eTriangleList,
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
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable         = false,
        .lineWidth               = 1.0f,
    };
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = false,
    };
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable    = false,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable   = false,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
    };
    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable       = vk::True,
        .depthWriteEnable      = vk::True,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable     = vk::False,
    };

    pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts    = &*descriptorSetLayout,
    });

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swapchainFormat,
        .depthAttachmentFormat   = depthFormat,
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
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = &dynamicState,
        .layout              = *pipelineLayout,
    });

    Log::Info("Graphics pipeline created (depth test, format %s).",
              vk::to_string(depthFormat).c_str());
    return true;
}
