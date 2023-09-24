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
	std::ifstream file{ filename, std::ios::binary | std::ios::ate };
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
	imageRegion.imageOffset = vk::Offset3D{ 0, 0, 0 };
	// Size of region to copy (xyz values)
	imageRegion.imageExtent = vk::Extent3D{ width, height, 1 };

	// Copy buffer to image
	transferCommandBuffer.copyBufferToImage(srcBuffer,
		dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &imageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool,
		transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(vk::Device device, vk::Queue queue, vk::CommandPool commandPool,
	vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
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
	imageMemoryBarrier.subresourceRange.levelCount = mipLevels;
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

static void generateMipmaps(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Queue queue, vk::CommandPool commandPool,
	vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if image format supports linear blitting. We create a texture image with 
	// the optimal tiling format, so we need to check optimalTilingFeatures.
	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);
	if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	vk::CommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	// The fields set below will remain the same for all barriers. 
	// On the contrary, subresourceRange.miplevel, oldLayout, newLayout, srcAccessMask,
	// and dstAccessMask will be changed for each transition.
	vk::ImageMemoryBarrier barrier{};
	barrier.image = image;
	barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	// This loop will record each of the blitImage commands. 
	// Note that the loop variable starts at 1, not 0.
	for (uint32_t i = 1; i < mipLevels; i++) 
	{
		// First, we transition level i - 1 to vk::ImageLayout::eTransferSrcOptimal. 
		// This transition will wait for level i - 1 to be filled, either from the previous blit command, 
		// or from vk::CommandBuffer::copyBufferToImage. 
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		// The current blit command will wait on this transition.
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, 0, nullptr, 1, &barrier);

		// Next, we specify the regions that will be used in the blit operation. The source mip level is i - 1 
		// and the destination mip level is i. The two elements of the srcOffsets array determine the 3D region 
		// that data will be blitted from. dstOffsets determines the region that data will be blitted to. 
		vk::ImageBlit blit{};
		blit.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
		blit.srcOffsets[1] = vk::Offset3D{ mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		// The X and Y dimensions of the dstOffsets[1] are divided by two since each mip level is 
		// half the size of the previous level.The Z dimension of srcOffsets[1] and dstOffsets[1] 
		// must be 1, since a 2D image has a depth of 1.
		blit.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
		blit.dstOffsets[1] = vk::Offset3D{ mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;


		// Now, we record the blit command. Note that textureImage is used for both the srcImage and dstImage 
		// parameter. This is because we're blitting between different levels of the same image. The source 
		// mip level was just transitioned to vk::ImageLayout::eTransferSrcOptimal and the destination level 
		// is still in vk::ImageLayout::eTransferDstOptimal from createTextureImage.
		// The last parameter is the same filtering options here that we had when making the vk::Sampler. 
		// We use the vk::Filter::eLinear to enable interpolation.
		commandBuffer.blitImage(
			image, vk::ImageLayout::eTransferSrcOptimal,
			image, vk::ImageLayout::eTransferDstOptimal,
			1, &blit,
			vk::Filter::eLinear);

		// This barrier transitions mip level i - 1 to vk::ImageLayout::eShaderReadOnlyOptimal. 
		// This transition waits on the current blit command to finish. All sampling operations will
		// wait on this transition to finish.
		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
			0, nullptr,
			0, nullptr,
			1, &barrier);

		// At the end of the loop, we divide the current mip dimensions by two. We check each 
		// dimension before the division to ensure that dimension never becomes 0. This handles 
		// cases where the image is not square, since one of the mip dimensions would reach 1 
		// before the other dimension. When this happens, that dimension should remain 1 for all 
		// remaining levels.
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Before we end the command buffer, we insert one more pipeline barrier. This barrier 
	// transitions the last mip level from vk::ImageLayout::eTransferDstOptimal to 
	// vk::ImageLayout::eShaderReadOnlyOptimal. This wasn't handled by the loop, since the 
	// last mip level is never blitted from.
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
		0, nullptr,
		0, nullptr,
		1, &barrier);

	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}