#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
using std::vector;

#include "VulkanUtilities.h"

class VulkanMesh
{
public:
	VulkanMesh(vk::PhysicalDevice physicalDeviceP, vk::Device deviceP, vector<Vertex>* vertices);
	VulkanMesh() = default;
	~VulkanMesh() = default;

	int getVextexCount();
	vk::Buffer getVertexBuffer();
	void destroyBuffers();

private:
	int vertexCount;
	vk::Buffer vertexBuffer;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::DeviceMemory vertexBufferMemory;

	void createVertexBuffer(vector<Vertex>* vertices);
	uint32_t findMemoryTypeIndex(vk::PhysicalDevice physicalDevice, uint32_t allowedTypes, vk::MemoryPropertyFlags properties);
};