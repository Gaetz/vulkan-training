#include "VulkanMesh.h"

VulkanMesh::VulkanMesh(vk::PhysicalDevice physicalDeviceP,
	vk::Device deviceP, vector<Vertex>* vertices):
	vertexCount(vertices->size()), physicalDevice(physicalDeviceP), device(deviceP)
{
	createVertexBuffer(vertices);
}

int VulkanMesh::getVextexCount()
{
	return vertexCount;
}

vk::Buffer VulkanMesh::getVertexBuffer()
{
	return vertexBuffer;
}

void VulkanMesh::destroyBuffers()
{
	device.destroyBuffer(vertexBuffer, nullptr);
	device.freeMemory(vertexBufferMemory, nullptr);
}

void VulkanMesh::createVertexBuffer(vector<Vertex>* vertices)
{
	// -- CREATE VERTEX BUFFER --
	// Buffer info
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.size = sizeof(Vertex) * vertices->size();
	// Multiple types of buffers
	bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	// Is vertex buffer sharable ? Here: no.
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	vertexBuffer = device.createBuffer(bufferInfo);

	// Get buffer memory requirements
	vk::MemoryRequirements memoryRequirements;
	device.getBufferMemoryRequirements(vertexBuffer, &memoryRequirements);

	// Allocate memory to buffer
	vk::MemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(
		physicalDevice,
		// Index of memory type on physical device that has requiered bit flags
		memoryRequirements.memoryTypeBits,
		// CPU can interact with memory
		vk::MemoryPropertyFlagBits::eHostVisible |
		// Allows placement of data straight into buffer after mapping
		vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// Allocate memory to vk::DeviceMemory
	auto result = device.allocateMemory(&memoryAllocInfo, nullptr, &vertexBufferMemory);
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	// Allocate memory to given vertex buffer
	device.bindBufferMemory(vertexBuffer, vertexBufferMemory, 0);

	// -- MAP MEMORY TO VERTEX BUFFER --
	// 1. Create pointer to a random point in memory
	void* data;
	// 2. Map the vertex buffer memory to that point
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);
	// 3. Copy memory from vertices memory to the point
	memcpy(data, vertices->data(), static_cast<size_t>(bufferInfo.size));
	// 4. Unmap the vertex buffer memory
	vkUnmapMemory(device, vertexBufferMemory);
	// Because we have eHostCoherent, we don't have to flush
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