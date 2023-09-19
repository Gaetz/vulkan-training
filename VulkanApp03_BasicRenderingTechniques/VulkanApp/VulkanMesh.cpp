#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(VkPhysicalDevice physicalDeviceP, VkDevice deviceP, 
	VkQueue transferQueue, VkCommandPool transferCommandPool, 
	vector<Vertex>* vertices, vector<uint32_t>* indices, int texIdP)
	:
	vertexCount(vertices->size()), physicalDevice(physicalDeviceP), 
	device(deviceP), indexCount(indices->size()), texId(texIdP)
{
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);
	model.model = glm::mat4(1.0f);
}

VulkanMesh::VulkanMesh()
{
}

VulkanMesh::~VulkanMesh()
{
}

int VulkanMesh::getVextexCount()
{
	return vertexCount;
}

VkBuffer VulkanMesh::getVertexBuffer()
{
	return vertexBuffer;
}

int VulkanMesh::getIndexCount()
{
	return indexCount;
}

VkBuffer VulkanMesh::getIndexBuffer()
{
	return indexBuffer;
}

void VulkanMesh::destroyBuffers()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}

void VulkanMesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, vector<Vertex>* vertices)
{
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

	// Temporary buffer to stage vertex data before transfering to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);												

	// Map memory to staging buffer
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices->data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device, stagingBufferMemory);

	// Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data
	// Buffer memory need to be DEVICE_LOCAL_BIT meaning memory is on GPU only and not CPU-accessible
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&vertexBuffer, &vertexBufferMemory);

	// Copy staging buffer to vertex buffer on GPU
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	// Clean staging buffer
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanMesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, vector<uint32_t>* indices)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device, stagingBufferMemory);

	// This time with INDEX_BUFFER_BIT, &indexBuffer and &indexBufferMemory
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&indexBuffer, &indexBufferMemory);

	// Copy to indexBuffer
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}
