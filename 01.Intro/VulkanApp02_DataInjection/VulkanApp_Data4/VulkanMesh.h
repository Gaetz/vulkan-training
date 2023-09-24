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
	VulkanMesh(vk::PhysicalDevice physicalDeviceP, vk::Device deviceP,
		vk::Queue transferQueue, vk::CommandPool transferCommandPool, 
		vector<Vertex>* vertices, vector<uint32_t>* indices);
	VulkanMesh() = default;
	~VulkanMesh() = default;

	size_t getVextexCount();
	vk::Buffer getVertexBuffer();
	size_t getIndexCount();
	vk::Buffer getIndexBuffer();

	Model getModel() const { return model; }
	void setModel(const glm::mat4& modelP) { model.model = modelP; }


	void destroyBuffers();

private:
	size_t vertexCount{0};
	size_t indexCount{0};
	Model model;


	vk::Buffer vertexBuffer;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::DeviceMemory vertexBufferMemory;
	vk::Buffer indexBuffer;
	vk::DeviceMemory indexBufferMemory;

	void createVertexBuffer(vk::Queue transferQueue, vk::CommandPool transferCommandPool, vector<Vertex>* vertices);
	void createIndexBuffer(vk::Queue transferQueue, vk::CommandPool transferCommandPool, vector<uint32_t>* indices);
	uint32_t findMemoryTypeIndex(vk::PhysicalDevice physicalDevice, uint32_t allowedTypes, vk::MemoryPropertyFlags properties);
};