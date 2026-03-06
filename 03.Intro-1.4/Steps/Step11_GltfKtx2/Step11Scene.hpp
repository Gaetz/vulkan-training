#pragma once

#include <glm/glm.hpp>

#include "../../Engine/IScene.hpp"
#include "../../Engine/Renderer/Model.hpp"
#include "../../Engine/Renderer/Texture.hpp"

// Step 11 — GLTF model + KTX2 texture
// Same animation and camera as Step 09, but assets are loaded via
// GltfModelLoader and KtxTextureLoader instead of tinyobjloader and stb_image.
class Step11Scene : public IScene {
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
