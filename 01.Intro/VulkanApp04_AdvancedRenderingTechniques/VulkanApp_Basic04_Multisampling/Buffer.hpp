#pragma once
#include <vector>
#include "vulkan/vulkan.hpp"


class Buffer
{
public:
	Buffer() = default;

	Buffer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize bufferSize,
		vk::BufferUsageFlags bufferUsage, vk::MemoryPropertyFlags bufferProperties);

	vk::Device device;
	vk::Buffer buffer = nullptr;
	vk::DeviceMemory memory = nullptr;
	vk::DescriptorBufferInfo descriptor;
	vk::DeviceSize size = 0;
	vk::DeviceSize alignment = 0;

	/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
	vk::BufferUsageFlags usageFlags;

	/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
	vk::MemoryPropertyFlags memoryPropertyFlags;

	/** @brief Store buffer data */
	void* mapped = nullptr;

	/*
	* Build the buffer with the specified parameters. 
	* Can be called after the default constructor to build the buffer.
	* 
	* @param device Logical device that creates the buffer
	* @param physicalDevice Physical device that is used to query properties of the buffer
	* @param size Size of the buffer in bytes	
	* @param usage Usage flag for the buffer (i.e. index buffer, vertex buffer, etc.)
	* @param properties Memory properties for this buffer
	*/
	void create(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize size, 
		vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

	/**
	* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
	*
	* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return VkResult of the buffer mapping call
	*/
	vk::Result map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

	/**
	* Unmap a mapped memory range
	*
	* @note Does not return a result as vkUnmapMemory can't fail
	*/
	void unmap();

	/**
	* Attach the allocated memory block to the buffer
	*
	* @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
	*/
	void bind(vk::DeviceSize offset = 0);

	/**
	* Setup the default descriptor for this buffer
	*
	* @param size (Optional) Size of the memory range of the descriptor
	* @param offset (Optional) Byte offset from beginning
	*
	*/
	void setupDescriptor(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

	/**
	* Copies the specified data to the mapped buffer
	*
	* @param data Pointer to the data to copy
	* @param size Size of the data to copy in machine units
	*
	*/
	void copyTo(void* data, vk::DeviceSize size);

	/**
	* Copy buffer to another buffer
	* 
	* @param dstBuffer Buffer to copy to
	* @param size Size of data to copy
	* @param transferQueue Queue used to copy the buffer
	* @param transferCommandPool Command pool used to allocate the copy command buffer
	*/
	void copyToBuffer(Buffer& dstBuffer, vk::DeviceSize size, 
		vk::Queue transferQueue, vk::CommandPool transferCommandPool);

	/**
	* Copy buffer to an image
	* 
	* @param dstImage Image to copy to
	* @param width Width of the image to copy to
	* @param height Height of the image to copy to
	* @param transferQueue Queue used to copy the buffer
	* @param transferCommandPool Command pool used to allocate the copy command buffer
	*/
	void copyToImage(vk::Image dstImage, uint32_t width, uint32_t height, vk::Queue transferQueue, vk::CommandPool transferCommandPool);

	/**
	* Flush a memory range of the buffer to make it visible to the device
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*/
	void flush(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);


	/**
	* Invalidate a memory range of the buffer to make it visible to the host
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*/
	void invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

	/**
	* Release all Vulkan resources held by this buffer
	*/
	void destroy();

};

