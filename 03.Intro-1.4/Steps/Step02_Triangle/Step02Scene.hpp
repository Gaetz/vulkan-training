#pragma once

#include "../../Engine/IScene.hpp"

// Step 02 - Triangle rendering
class Step02Scene : public IScene {
public:
    bool init(IRenderer& renderer) override { return true; }
    void update(float deltaTime) override {}
    void cleanup() override {}
};
