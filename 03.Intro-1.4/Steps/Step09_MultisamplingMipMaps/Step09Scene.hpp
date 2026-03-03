#pragma once

#include <glm/glm.hpp>

#include "../../Engine/IScene.hpp"
#include "../../Engine/Renderer/Model.hpp"
#include "../../Engine/Renderer/Texture.hpp"

// Step 09 — Multisampling + Mipmaps
// Same animation and camera as Steps 06/07/08.
// The Scene owns the 3D model and texture (the "what to draw");
// the Renderer owns the GPU pipeline (the "how to draw").
class Step09Scene : public IScene {
public:
    bool      init(IRenderer& renderer) override;
    void      update(float deltaTime) override;
    void      cleanup() override;

    glm::mat4      getModelMatrix() const { return modelMatrix; }
    glm::mat4      getViewMatrix()  const { return view; }
    const Model&   getModel()       const { return model; }
    const Texture& getTexture()     const { return texture; }

private:
    float     elapsedTime = 0.0f;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::mat4 view        = glm::mat4(1.0f);

    Model   model;
    Texture texture;
};
