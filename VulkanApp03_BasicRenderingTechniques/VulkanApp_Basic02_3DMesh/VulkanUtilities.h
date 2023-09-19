#pragma once
#include <iostream>
#include <fstream>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <vector>
using std::vector;
#include <string>
using std::string;

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 col;
	glm::vec2 tex;
};

const vector<const char*> deviceExtensions
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Indices (locations) of Queue Families, if they exist
struct QueueFamilyIndices 
{
	int graphicsFamily = -1;		// Location of Graphics Queue Family
	int presentationFamily = -1;	// Location of Presentation Queue Family

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
	// What the surface is capable of displaying, e.g. image size/extent
	vk::SurfaceCapabilitiesKHR surfaceCapabilities;		
	// Vector of the image formats, e.g. RGBA
	vector<vk::SurfaceFormatKHR> formats;		
	// Vector of presentation modes
	vector<vk::PresentModeKHR> presentationModes;			
};

struct SwapchainImage
{
	vk::Image image;
	vk::ImageView imageView;
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

static uint32_t findMemoryTypeIndex(vk::PhysicalDevice physicalDevice,
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

static void createBuffer(vk::PhysicalDevice physicalDevice, vk::Device device,
	vk::DeviceSize bufferSize, vk::BufferUsageFlags bufferUsage,
	vk::MemoryPropertyFlags bufferProperties, vk::Buffer* buffer,
	vk::DeviceMemory* bufferMemory)
{
	// Buffer info
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsage;							// Multiple types of buffers
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;	// Is vertex buffer sharable ? Here: no.

	*buffer = device.createBuffer(bufferInfo);

	// Get buffer memory requirements
	vk::MemoryRequirements memoryRequirements;
	device.getBufferMemoryRequirements(*buffer, &memoryRequirements);

	// Allocate memory to buffer
	vk::MemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(
		physicalDevice,
		// Index of memory type on physical device that has required bit flags
		memoryRequirements.memoryTypeBits,
		bufferProperties
	);

	// Allocate memory to VkDeviceMemory
	auto result = device.allocateMemory(&memoryAllocInfo, nullptr, bufferMemory);
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	// Allocate memory to given vertex buffer
	device.bindBufferMemory(*buffer, *bufferMemory, 0);
}

static vk::CommandBuffer beginCommandBuffer(vk::Device device, vk::CommandPool commandPool)
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer commandBuffer;

	// Command buffer details
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	commandBuffer = device.allocateCommandBuffers(allocInfo).front();

	// Information to begin command buffer record
	vk::CommandBufferBeginInfo beginInfo{};
	// Only using command buffer once, then become unvalid
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	// Begin records transfer commands
	commandBuffer.begin(beginInfo);
	return commandBuffer;
}

static void endAndSubmitCommandBuffer(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, vk::CommandBuffer commandBuffer)
{
	// End record commands
	commandBuffer.end();

	// Queue submission info
	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Submit transfer commands to transfer queue and wait until it finishes
	queue.submit(1, &submitInfo, nullptr);
	queue.waitIdle();

	// Free temporary command buffer
	device.freeCommandBuffers(commandPool, 1, &commandBuffer);
}

static void copyBuffer(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
	vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize bufferSize)
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	// Region of data to copy from and to
	vk::BufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;		// From the start of first buffer...
	bufferCopyRegion.dstOffset = 0;		// ...copy to the start of second buffer
	bufferCopyRegion.size = bufferSize;

	// Copy src buffer to dst buffer
	transferCommandBuffer.copyBuffer(srcBuffer, dstBuffer, bufferCopyRegion);

	// Submit and free
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(vk::Device device, vk::Queue transferQueue,
	vk::CommandPool transferCommandPool, vk::Buffer srcBuffer, vk::Image dstImage,
	uint32_t width, uint32_t height)
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
	imageRegion.imageOffset = vk::Offset3D { 0, 0, 0 };
	// Size of region to copy (xyz values)
	imageRegion.imageExtent = vk::Extent3D { width, height, 1 };

	// Copy buffer to image
	transferCommandBuffer.copyBufferToImage(srcBuffer,
		dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &imageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool,
		transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(vk::Device device, vk::Queue queue, vk::CommandPool commandPool,
	vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	vk::CommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	vk::ImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.oldLayout = oldLayout;
	imageMemoryBarrier.newLayout = newLayout;
	// Queue family to transition from
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// Queue family to transition to
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// Image being accessed and modified as part fo barrier
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	// First mip level to start alterations on
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	// Number of mip levels to alter starting from baseMipLevel
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	// First layer to starts alterations on
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	// Number of layers to alter starting from baseArrayLayer
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;

	// If transitioning from new image to image ready to receive data
	if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		// Memory access stage transition must happen after this stage
		imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNone;
		// Memory access stage transition must happen before this stage
		imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		// Transfer from old layout to new layout has to occur after any
		// point of the top of the pipeline and before it attemps to to a
		// transfer write at the transfer stage of the pipeline
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		// Transfer is finished
		imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		// Before the shader reads
		imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}

	commandBuffer.pipelineBarrier(
		srcStage, dstStage,		// Pipeline stages (match to src and dst AccessMasks)
		{},						// Dependency flags
		0, nullptr,				// Memory barrier count and data
		0, nullptr,				// Buffer memory barrier count and data
		1, &imageMemoryBarrier	// Image memory barrier count and data
	);

	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}