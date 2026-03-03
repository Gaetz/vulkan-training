#pragma once

#include <glm/glm.hpp>

#include "../../Engine/IScene.hpp"

// Step 08 — Loading Models
// Same animation and camera as Steps 06/07.
class Step08Scene : public IScene {
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
