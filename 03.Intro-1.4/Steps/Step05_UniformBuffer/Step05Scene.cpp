#include "Step05Scene.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

bool Step05Scene::init(IRenderer& /*renderer*/) {
    return true;
}

void Step05Scene::update(float deltaTime) {
    elapsedTime += deltaTime;
    model = glm::rotate(glm::mat4(1.0f),
                         elapsedTime * glm::radians(90.0f),
                         glm::vec3(0.0f, 0.0f, 1.0f));
    view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 0.0f, 1.0f));
}

void Step05Scene::cleanup() {
    elapsedTime = 0.0f;
}
