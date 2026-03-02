#pragma once

#include <glm/glm.hpp>

#include "../../Engine/IScene.hpp"

// Step 05 — Uniform buffer
//
// The scene owns animation and camera state (model + view).
// Projection is the renderer's responsibility (it knows the swapchain extent).
class Step05Scene : public IScene {
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
