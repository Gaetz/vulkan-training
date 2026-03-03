#include "Step09Scene.hpp"
#include "Step09Renderer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

bool Step09Scene::init(IRenderer& renderer) {
    auto& r = static_cast<Step09Renderer&>(renderer);

    model = r.loadModel("assets/models/viking_room.obj");
    texture = r.loadTexture("assets/textures/viking_room.png");
    return model.valid() && texture.valid();
}

void Step09Scene::update(float deltaTime) {
    elapsedTime += deltaTime;
    modelMatrix = glm::rotate(glm::mat4(1.0f),
                               elapsedTime * glm::radians(90.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
    view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 0.0f, 1.0f));
}

void Step09Scene::cleanup() {
    model.destroy();
    texture.destroy();
    elapsedTime = 0.0f;
}
