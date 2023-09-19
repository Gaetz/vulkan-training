#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <glm/glm.hpp>

#include <vector>
using std::vector;
#include <string>
using std::string;

const vector<const char*> deviceExtensions
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Indices (locations) of Queue Families, if they exist
struct QueueFamilyIndices 
{
	int graphicsFamily = -1;		// Location of Graphics Queue Family
	int presentationFamily = -1;	// Location of PResentation Queue Family

	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

struct SwapchainDetails 
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;		// What the surface is capable of displaying, e.g. image size/extent
	vector<VkSurfaceFormatKHR> formats;					// Vector of the image formats, e.g. RGBA
	vector<VkPresentModeKHR> presentationModes;			// Vector of presentation modes
};

struct SwapchainImage
{
	VkImage image;
	VkImageView imageView;
};

static vector<char> readShaderFile(const string& filename)
{
	// Open shader file
	// spv files are binary data, put the pointer at the end of the file to get its size
	std::ifstream file { filename, std::ios::binary | std::ios::ate };
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open a file");
	}

	// Buffer preparation
	size_t fileSize = (size_t)file.tellg();		// Get the size through the position of the pointer
	vector<char> fileBuffer(fileSize);			// Set file buffer to the file size
	file.seekg(0);								// Move in file to start of the file

	// Reading and closing
	file.read(fileBuffer.data(), fileSize);
	file.close();
	return fileBuffer;
}

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 col;
	glm::vec2 tex;
};

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	// Get properties of physical device
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		// We iterate through each bit, shifting of 1 (with i) each time.
		// This way we go through each type to check it is allowed.
		if ((allowedTypes & (1 << i))
			// Desired property bit flags are part of memory type's property flags.
			// By checking the equality, we check that all properties are available at the same time,
			// and not only one property is common.
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			// If this type is an allowed type and has the flags we want,
			// then i is the current index of the memory type we want to use. Return it.
			return i;
		}
	}
}

static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, 
	VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	// Buffer info
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsage;							// Multiple types of buffers
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Is vertex buffer sharable ? Here: no.

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a mesh vertex buffer.");
	}

	// Get buffer memory requirements
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	// Allocate memory to buffer
	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(
		physicalDevice,
		memoryRequirements.memoryTypeBits,			// Index of memory type on physical device that has requiered bit flags
		bufferProperties
	);

	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	// Allocate memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	// Command buffer to hold transfer commands
	VkCommandBuffer commandBuffer;

	// Command buffer details
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	// Information to begin command buffer record
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// Only using command buffer once, then become unvalid

	// Begin records transfer commands
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	// End record commands
	vkEndCommandBuffer(commandBuffer);

	// Queue submission info
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Submit transfer commands to transfer queue and wait until it finishes
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	// Free temporary command buffer
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	// Create buffer
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	// Region of data to copy from and to
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;		// From the start of first buffer...
	bufferCopyRegion.dstOffset = 0;		// ...copy to the start of second buffer
	bufferCopyRegion.size = bufferSize;

	// Copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	// Submit and free
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height)
{
	// Create buffer
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	VkBufferImageCopy imageRegion{};
	// All data of image is tightly packed
	imageRegion.bufferOffset = 0;			// Offset into data
	imageRegion.bufferRowLength = 0;		// Row length of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;		// Image height of data to calculate data spacing

	imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// Which aspect to copy (here: colors)
	imageRegion.imageSubresource.mipLevel = 0;								// Mipmap level to copy
	imageRegion.imageSubresource.baseArrayLayer = 0;						// Starting array layer if array
	imageRegion.imageSubresource.layerCount = 1;							// Number of layers to copy starting at baseArray
	imageRegion.imageOffset = { 0, 0, 0 };									// Offset into image (as opposed to raw data into bufferOffset)
	imageRegion.imageExtent = { width, height, 1 };							// Size of region to copy (xyz values)

	// Copy buffer to image
	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldLayout;							
	imageMemoryBarrier.newLayout = newLayout;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	// Queue family to transition from
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	// Queue family to transition to
	imageMemoryBarrier.image = image;									// Image being accessed and modified as part fo barrier
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;				// First mip level to start alterations on
	imageMemoryBarrier.subresourceRange.levelCount = 1;					// Number of mip levels to alter starting from baseMipLevel
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;				// First layer to starts alterations on
	imageMemoryBarrier.subresourceRange.layerCount = 1;					// Number of layers to alter starting from baseArrayLayer

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	// If transitioning from new image to image ready to receive data
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;								// Memory access stage transition must happen after this stage
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	// Memory access stage transition must happen before this stage

		// Transfer from old layout to new layout has to occur after any point of the top of the pipeline
		// and before it attemps to to a transfer write at the transfer stage of the pipeline
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	// If transitioning from transfer destination to shader read
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	// Transfer is finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;		// Before the shader reads

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage, dstStage,		// Pipeline stages (match to src and dst AccessMasks)
		0,						// Dependency flags
		0, nullptr,				// Memory barrier count and data
		0, nullptr,				// Buffer memory barrier count and data
		1, &imageMemoryBarrier	// Image memory barrier count and data
		);

	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}