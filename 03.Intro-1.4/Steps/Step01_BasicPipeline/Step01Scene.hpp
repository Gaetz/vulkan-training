#pragma once

#include "../../Engine/IScene.hpp"

// Step 01 - Basic pipeline
class Step01Scene : public IScene {
public:
    bool init(IRenderer& renderer) override { return true; }
    void update(float deltaTime) override {}
    void cleanup() override {}
};
