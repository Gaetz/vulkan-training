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