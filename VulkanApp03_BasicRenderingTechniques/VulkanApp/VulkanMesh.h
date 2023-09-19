#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
using std::vector;

#include "VulkanUtilities.h"

struct Model {
	glm::mat4 model;
};

class VulkanMesh
{
public:
	VulkanMesh(VkPhysicalDevice physicalDeviceP, VkDevice deviceP,
		VkQueue transferQueue, VkCommandPool transferCommandPool,
		vector<Vertex>* vertices, vector<uint32_t>* indices, int texIdP);
	VulkanMesh();
	~VulkanMesh();

	int getVextexCount();
	VkBuffer getVertexBuffer();
	int getIndexCount();
	VkBuffer getIndexBuffer();
	void destroyBuffers();

	Model getModel() { return model; }
	void setModel(const glm::mat4& modelP) { model.model = modelP; }
	int getTexId() { return texId; }

private:
	int vertexCount;
	VkBuffer vertexBuffer;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkDeviceMemory vertexBufferMemory;

	int indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	Model model;
	int texId;

	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, vector<Vertex>* vertices);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, vector<uint32_t>* indices);
};

