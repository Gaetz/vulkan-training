#include "ImmediateSubmit.hpp"

ImmediateSubmit::ImmediateSubmit(vk::raii::Device&      device,
                                  vk::raii::CommandPool& commandPool,
                                  vk::raii::Queue&       queue)
    : device(device), commandPool(commandPool), queue(queue)
{}

void ImmediateSubmit::operator()(std::function<void(vk::CommandBuffer)> fn) {
    auto cmds = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        .commandPool        = *commandPool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    vk::raii::CommandBuffer cmd = std::move(cmds[0]);

    cmd.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    fn(*cmd);
    cmd.end();

    vk::CommandBuffer raw = *cmd;
    queue.submit(vk::SubmitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers    = &raw,
    });
    queue.waitIdle();
}
