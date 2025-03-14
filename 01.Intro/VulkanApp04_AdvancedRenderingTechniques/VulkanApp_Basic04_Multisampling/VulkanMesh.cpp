#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(vk::PhysicalDevice physicalDeviceP,vk::Device deviceP, 
	vk::Queue transferQueue, vk::CommandPool transferCommandPool, 
	vector<Vertex>* vertices, vector<uint32_t>* indices, int texIdP)
	:
	vertexCount{ vertices->size() }, indexCount{ indices->size() },
	physicalDevice{ physicalDeviceP }, device{ deviceP }, texId{ texIdP }
{
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);
	model.model = glm::mat4(1.0f);
}

size_t VulkanMesh::getVextexCount()
{
	return vertexCount;
}

vk::Buffer VulkanMesh::getVertexBuffer()
{
	return vertexBuffer.buffer;
}

size_t VulkanMesh::getIndexCount()
{
	return indexCount;
}

vk::Buffer VulkanMesh::getIndexBuffer()
{
	return indexBuffer.buffer;
}

void VulkanMesh::destroyBuffers()
{
	vertexBuffer.destroy();
	indexBuffer.destroy();
}

void VulkanMesh::createVertexBuffer(vk::Queue transferQueue, vk::CommandPool transferCommandPool, vector<Vertex>* vertices)
{
	vk::DeviceSize bufferSize = sizeof(Vertex) * vertices->size();

	// Temporary buffer to stage vertex data before transfering to GPU
	Buffer stagingBuffer{ device, physicalDevice, bufferSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent };

	// Map memory to staging buffer
	stagingBuffer.map(bufferSize, 0);
	stagingBuffer.copyTo(vertices->data(), bufferSize);
	stagingBuffer.unmap();

	// Create buffer with vk::BufferUsageFlagBits::eTransferDst to mark as recipient of transfer data
	// Buffer memory need to be vk::MemoryPropertyFlagBits::eDeviceLocal meaning memory is on GPU only
	// and not CPU-accessible
	vertexBuffer.create(device, physicalDevice, bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Copy staging buffer to vertex buffer on GPU
	stagingBuffer.copyToBuffer(vertexBuffer, bufferSize, transferQueue, transferCommandPool);

	// Clean staging buffer
	stagingBuffer.destroy();
}

void VulkanMesh::createIndexBuffer(vk::Queue transferQueue,
	vk::CommandPool transferCommandPool, vector<uint32_t>* indices)
{
	vk::DeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	Buffer stagingBuffer{ device, physicalDevice, bufferSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent };

	stagingBuffer.map(bufferSize, 0);
	stagingBuffer.copyTo(indices->data(), bufferSize);
	stagingBuffer.unmap();

	// This time with vk::BufferUsageFlagBits::eIndexBuffer
	indexBuffer.create(device, physicalDevice, bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Copy to indexBuffer
	stagingBuffer.copyToBuffer(indexBuffer, bufferSize, transferQueue, transferCommandPool);
	
	// Clean staging buffer
	stagingBuffer.destroy();
}

uint32_t VulkanMesh::findMemoryTypeIndex(vk::PhysicalDevice physicalDevice,
	uint32_t allowedTypes, vk::MemoryPropertyFlags properties)
{
	// Get properties of physical device
	vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		// We iterate through each bit, shifting of 1 (with i) each time.
		// This way we go through each type to check it is allowed.
		if ((allowedTypes & (1 << i))
			// Desired property bit flags are part of memory type's property flags.
			// By checking the equality, we check that all properties are available at the
			// same time, and not only one property is common.
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			// If this type is an allowed type and has the flags we want,
			// then i is the current index of the memory type we want to use. Return it.
			return i;
		}
	}
}