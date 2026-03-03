#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <functional>

// Executes a single-time command buffer on a queue and waits for completion.
//
// Wraps the common pattern:
//   allocate one-shot cmd buf → begin(eOneTimeSubmit) → record → end → submit → waitIdle
//
// Usage:
//   ImmediateSubmit submit{device, commandPool, graphicsQueue};
//   submit([&](vk::CommandBuffer cmd) {
//       cmd.copyBuffer(src, dst, vk::BufferCopy{.size = size});
//   });
//
// The object stores references — it must not outlive its arguments.
class ImmediateSubmit {
public:
    ImmediateSubmit(vk::raii::Device&      device,
                    vk::raii::CommandPool& commandPool,
                    vk::raii::Queue&       queue);

    void operator()(std::function<void(vk::CommandBuffer)> fn);

private:
    vk::raii::Device&      device;
    vk::raii::CommandPool& commandPool;
    vk::raii::Queue&       queue;
};
