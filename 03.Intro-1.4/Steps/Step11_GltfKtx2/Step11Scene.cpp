#include "Step11Scene.hpp"
#include "Step11Renderer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

bool Step11Scene::init(IRenderer& renderer) {
    auto& r = static_cast<Step11Renderer&>(renderer);

    model   = r.loadGltfModel ("assets/models/viking_room.glb");
    texture = r.loadKtxTexture("assets/textures/viking_room.ktx2");
    return model.valid() && texture.valid();
}

void Step11Scene::update(float deltaTime) {
    elapsedTime += deltaTime;
    // GLTF is Y-up: rotate around Y axis, no base correction needed.
    modelMatrix = glm::rotate(glm::mat4(1.0f),
                               elapsedTime * glm::radians(90.0f),
                               glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f));
}

void Step11Scene::cleanup() {
    model.destroy();
    texture.destroy();
    elapsedTime = 0.0f;
}
