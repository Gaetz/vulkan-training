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
#include "Steps/Step01_BasicPipeline/Step01Renderer.hpp"
#include "Steps/Step01_BasicPipeline/Step01Scene.hpp"
#include "Steps/Step02_Triangle/Step02Renderer.hpp"
#include "Steps/Step02_Triangle/Step02Scene.hpp"
#include "Steps/Step03_SwapchainRecreation/Step03Renderer.hpp"
#include "Steps/Step03_SwapchainRecreation/Step03Scene.hpp"
#include "Steps/Step04_VertexBuffer/Step04Renderer.hpp"
#include "Steps/Step04_VertexBuffer/Step04Scene.hpp"
#include "Steps/Step05_UniformBuffer/Step05Renderer.hpp"
#include "Steps/Step05_UniformBuffer/Step05Scene.hpp"
#include "Steps/Step06_Textures/Step06Renderer.hpp"
#include "Steps/Step06_Textures/Step06Scene.hpp"
#include "Steps/Step07_Depth/Step07Renderer.hpp"
#include "Steps/Step07_Depth/Step07Scene.hpp"
#include "Steps/Step08_Modele3D/Step08Renderer.hpp"
#include "Steps/Step08_Modele3D/Step08Scene.hpp"

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

int main(int argc, char* argv[]) {
    Application app("Vulkan with SDL3", WINDOW_WIDTH, WINDOW_HEIGHT);

    if (!app.init()) {
        return 1;
    }

    app.getEngine().setRenderer(std::make_unique<Step08Renderer>());
    app.getEngine().setScene(std::make_unique<Step08Scene>());
    if (!app.getEngine().init()) return 1;

    app.mainLoop();
    app.cleanup();

    return 0;
}
