#pragma once

#include "../../Engine/IScene.hpp"

// Step 04 — Vertex buffer + index buffer
class Step04Scene : public IScene {
public:
    bool init(IRenderer& renderer) override { return true; }
    void update(float deltaTime) override {}
    void cleanup() override {}
};
