#include "Application.hpp"

// --- Bibliothèques une seule fois ici (implémentations header-only) ---

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// --- Scenes & Renderers — one pair per tutorial step ---
#include "Steps/Step00_Setup/Step00Renderer.hpp"
#include "Steps/Step00_Setup/Step00Scene.hpp"

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

int main(int argc, char* argv[]) {
    Application app("Vulkan with SDL3", WINDOW_WIDTH, WINDOW_HEIGHT);

    if (!app.init()) {
        return 1;
    }

    app.getEngine().setRenderer(std::make_unique<Step00Renderer>());
    app.getEngine().setScene(std::make_unique<Step00Scene>());
    if (!app.getEngine().init()) return 1;

    app.mainLoop();
    app.cleanup();

    return 0;
}
