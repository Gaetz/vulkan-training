#pragma once

#include <glm/glm.hpp>

#include "../../Engine/IScene.hpp"

// Step 06 — Textures
// Scene owns animation and camera (model + view).
// Projection is the renderer's responsibility (it knows the swapchain extent).
class Step06Scene : public IScene {
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
