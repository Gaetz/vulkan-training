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

static void copyBuffer(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
	vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize bufferSize)
{
	// Command buffer to hold transfer commands
	vk::CommandBuffer transferCommandBuffer;

	// Command buffer details
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	transferCommandBuffer = device.allocateCommandBuffers(allocInfo).front();

	// Information to begin command buffer record
	vk::CommandBufferBeginInfo beginInfo{};
	// Only using command buffer once, then become unvalid
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	// Begin records transfer commands
	transferCommandBuffer.begin(beginInfo);

	// Region of data to copy from and to
	vk::BufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;		// From the start of first buffer...
	bufferCopyRegion.dstOffset = 0;		// ...copy to the start of second buffer
	bufferCopyRegion.size = bufferSize;

	// Copy src buffer to dst buffer
	transferCommandBuffer.copyBuffer(srcBuffer, dstBuffer, bufferCopyRegion);

	// End record commands
	transferCommandBuffer.end();

	// Queue submission info
	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	// Submit transfer commands to transfer queue and wait until it finishes
	transferQueue.submit(submitInfo);
	transferQueue.waitIdle();

	// Free temporary command buffer
	device.freeCommandBuffers(transferCommandPool, transferCommandBuffer);
}