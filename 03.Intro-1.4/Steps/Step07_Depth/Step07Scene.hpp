#pragma once

#include <glm/glm.hpp>

#include "../../Engine/IScene.hpp"

// Step 07 — Depth Buffering
// Same animation and camera as Step 06.
// The renderer now renders two overlapping squares at different depths.
class Step07Scene : public IScene {
public:
    bool      init(IRenderer& renderer) override;
    void      update(float deltaTime) override;
    void      cleanup() override;

    glm::mat4 getModelMatrix() const { return model; }
    glm::mat4 getViewMatrix()  const { return view; }

private:
    float     elapsedTime = 0.0f;
    glm::mat4 model       = glm::mat4(1.0f);
    glm::mat4 view        = glm::mat4(1.0f);
};
