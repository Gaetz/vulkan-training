#pragma once

#include "../../Engine/IScene.hpp"

// Step 03 — Swapchain recreation
class Step03Scene : public IScene {
public:
    bool init(IRenderer& renderer) override { return true; }
    void update(float deltaTime) override {}
    void cleanup() override {}
};
