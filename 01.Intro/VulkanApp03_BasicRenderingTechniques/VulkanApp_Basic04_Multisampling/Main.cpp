#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define STB_IMAGE_IMPLEMENTATION

#define DWORD unsigned int
#if defined(WIN32) || defined(_WIN32)
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}
extern "C"
{
	__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#else
extern "C"
{
	int NvOptimusEnablement = 1;
}
extern "C"
{
	int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>

#include <string>
using std::string;

#include "VulkanRenderer.h"

GLFWwindow *window = nullptr;
VulkanRenderer vulkanRenderer;

void initWindow(string wName = "Vulkan", const int width = 800, const int height = 600)
{
	// Initialize GLFW
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Glfw won't work with opengl
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

void clean()
{
	vulkanRenderer.clean();
	glfwDestroyWindow(window);
	glfwTerminate();
}

int main()
{
	initWindow();
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
		return EXIT_FAILURE;

	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	// Load model
	int modelId = vulkanRenderer.createMeshModel("models/Futuristic combat jet.obj");

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f) { angle -= 360.0f; }

		glm::mat4 rotationModelMatrix(1.0f);
		rotationModelMatrix = glm::translate(rotationModelMatrix, glm::vec3(-0.0f, 0.0f, -1.0f));
		rotationModelMatrix = glm::rotate(rotationModelMatrix, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
		vulkanRenderer.updateModel(modelId, rotationModelMatrix);

		vulkanRenderer.draw();
	}

	clean();
	return 0;
}