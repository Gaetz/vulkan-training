#include "Buffer.hpp"
#include "VulkanUtilities.h"

Buffer::Buffer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize bufferSize,
	vk::BufferUsageFlags bufferUsage, vk::MemoryPropertyFlags bufferProperties)
{
	create(device, physicalDevice, bufferSize, bufferUsage, bufferProperties);
}

void Buffer::create(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	this->device = device;
	this->size = size;
	this->usageFlags = usage;
	this->memoryPropertyFlags = properties;

	// Buffer info
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.size = size;
	bufferInfo.usage = usageFlags;							// Multiple types of buffers
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;	// Is vertex buffer sharable ? Here: no.

	buffer = device.createBuffer(bufferInfo);

	// Get buffer memory requirements
	vk::MemoryRequirements memoryRequirements;
	device.getBufferMemoryRequirements(buffer, &memoryRequirements);

	// Allocate memory to buffer
	vk::MemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(
		physicalDevice,
		// Index of memory type on physical device that has required bit flags
		memoryRequirements.memoryTypeBits,
		memoryPropertyFlags
	);

	// Allocate memory to VkDeviceMemory
	auto result = device.allocateMemory(&memoryAllocInfo, nullptr, &memory);
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	// Allocate memory to given vertex buffer
	device.bindBufferMemory(buffer, memory, 0);
}

vk::Result Buffer::map(vk::DeviceSize size, vk::DeviceSize offset)
{
	return device.mapMemory(memory, offset, size, {}, &mapped);
}

void Buffer::unmap()
{
	if (mapped)
	{
		device.unmapMemory(memory);
		mapped = nullptr;
	}
}

void Buffer::bind(vk::DeviceSize offset)
{
	device.bindBufferMemory(buffer, memory, offset);
}


void Buffer::setupDescriptor(vk::DeviceSize size, vk::DeviceSize offset)
{
	descriptor.offset = offset;
	descriptor.buffer = buffer;
	descriptor.range = size;
}


void Buffer::copyTo(void* data, vk::DeviceSize size)
{
	assert(mapped);
	memcpy(mapped, data, size);
}

void Buffer::copyToBuffer(Buffer& dstBuffer, vk::DeviceSize size, vk::Queue transferQueue, vk::CommandPool transferCommandPool)
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	// Region of data to copy from and to
	vk::BufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;		// From the start of first buffer...
	bufferCopyRegion.dstOffset = 0;		// ...copy to the start of second buffer
	bufferCopyRegion.size = size;

	// Copy src buffer to dst buffer
	transferCommandBuffer.copyBuffer(buffer, dstBuffer.buffer, bufferCopyRegion);

	// Submit and free
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}


void Buffer::copyToImage(vk::Image dstImage, uint32_t width, uint32_t height, vk::Queue transferQueue, vk::CommandPool transferCommandPool)
{
	// Create buffer
	vk::CommandBuffer transferCommandBuffer =
		beginCommandBuffer(device, transferCommandPool);

	vk::BufferImageCopy imageRegion{};
	// All data of image is tightly packed
	// -- Offset into data
	imageRegion.bufferOffset = 0;
	// -- Row length of data to calculate data spacing
	imageRegion.bufferRowLength = 0;
	// -- Image height of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;

	// Which aspect to copy (here: colors)
	imageRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	// Mipmap level to copy
	imageRegion.imageSubresource.mipLevel = 0;
	// Starting array layer if array
	imageRegion.imageSubresource.baseArrayLayer = 0;
	// Number of layers to copy starting at baseArray
	imageRegion.imageSubresource.layerCount = 1;
	// Offset into image (as opposed to raw data into bufferOffset)
	imageRegion.imageOffset = vk::Offset3D{ 0, 0, 0 };
	// Size of region to copy (xyz values)
	imageRegion.imageExtent = vk::Extent3D{ width, height, 1 };

	// Copy buffer to image
	transferCommandBuffer.copyBufferToImage(buffer,
		dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &imageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool,
		transferQueue, transferCommandBuffer);
}

void Buffer::flush(vk::DeviceSize size, vk::DeviceSize offset)
{
	vk::MappedMemoryRange mappedRange {};
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	device.flushMappedMemoryRanges(mappedRange);
}

void Buffer::invalidate(vk::DeviceSize size, vk::DeviceSize offset)
{
	vk::MappedMemoryRange mappedRange {};
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	device.invalidateMappedMemoryRanges(mappedRange);
}

void Buffer::destroy()
{
	if (buffer)
	{
		device.destroyBuffer(buffer, nullptr);
	}
	if (memory)
	{
		device.freeMemory(memory, nullptr);
	}
}