#pragma once

#include "../../Engine/IScene.hpp"

// Step 00 — Setup
// There is nothing to render yet. The scene is a placeholder that satisfies
// the IScene interface while the renderer focus is purely on Vulkan
// initialization (instance, device, swapchain, image views).
class Step00Scene : public IScene {
public:
    bool init(IRenderer& renderer) override { return true; }
    void update(float deltaTime) override {}
    void cleanup() override {}
};
