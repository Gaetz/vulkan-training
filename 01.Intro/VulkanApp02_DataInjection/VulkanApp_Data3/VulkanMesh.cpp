#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(vk::PhysicalDevice physicalDeviceP,vk::Device deviceP, 
	vk::Queue transferQueue, vk::CommandPool transferCommandPool, 
	vector<Vertex>* vertices, vector<uint32_t>* indices)
	:
	vertexCount(vertices->size()), indexCount(indices->size()), 
	physicalDevice(physicalDeviceP), device(deviceP)
{
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);
}

size_t VulkanMesh::getVextexCount()
{
	return vertexCount;
}

vk::Buffer VulkanMesh::getVertexBuffer()
{
	return vertexBuffer;
}

size_t VulkanMesh::getIndexCount()
{
	return indexCount;
}

vk::Buffer VulkanMesh::getIndexBuffer()
{
	return indexBuffer;
}

void VulkanMesh::destroyBuffers()
{
	device.destroyBuffer(vertexBuffer, nullptr);
	device.freeMemory(vertexBufferMemory, nullptr);
	device.destroyBuffer(indexBuffer, nullptr);
	device.freeMemory(indexBufferMemory, nullptr);
}

void VulkanMesh::createVertexBuffer(vk::Queue transferQueue, vk::CommandPool transferCommandPool, vector<Vertex>* vertices)
{
	vk::DeviceSize bufferSize = sizeof(Vertex) * vertices->size();

	// Temporary buffer to stage vertex data before transfering to GPU
	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;

	createBuffer(physicalDevice, device, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		&stagingBuffer, &stagingBufferMemory);

	// Map memory to staging buffer
	void* data;
	device.mapMemory(stagingBufferMemory, {}, bufferSize, {}, &data);
	memcpy(data, vertices->data(), static_cast<size_t>(bufferSize));
	device.unmapMemory(stagingBufferMemory);


	// Create buffer with vk::BufferUsageFlagBits::eTransferDst to mark as recipient of transfer data
	// Buffer memory need to be vk::MemoryPropertyFlagBits::eDeviceLocal meaning memory is on GPU only
	// and not CPU-accessible
	createBuffer(physicalDevice, device, bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&vertexBuffer, &vertexBufferMemory);

	// Copy staging buffer to vertex buffer on GPU
	copyBuffer(device, transferQueue, transferCommandPool,
		stagingBuffer, vertexBuffer, bufferSize);

	// Clean staging buffer
	device.destroyBuffer(stagingBuffer, nullptr);
	device.freeMemory(stagingBufferMemory, nullptr);
}

void VulkanMesh::createIndexBuffer(vk::Queue transferQueue,
	vk::CommandPool transferCommandPool, vector<uint32_t>* indices)
{
	vk::DeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice, device, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		&stagingBuffer, &stagingBufferMemory);

	void* data;
	device.mapMemory(stagingBufferMemory, {}, bufferSize, {}, &data);
	memcpy(data, indices->data(), static_cast<size_t>(bufferSize));
	device.unmapMemory(stagingBufferMemory);

	// This time with vk::BufferUsageFlagBits::eIndexBuffer, &indexBuffer and &indexBufferMemory
	createBuffer(physicalDevice, device, bufferSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&indexBuffer, &indexBufferMemory);

	// Copy to indexBuffer
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	device.destroyBuffer(stagingBuffer);
	device.freeMemory(stagingBufferMemory);
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