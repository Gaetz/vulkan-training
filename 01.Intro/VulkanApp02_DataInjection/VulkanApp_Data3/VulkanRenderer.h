#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <vector>
using std::vector;
#include <set>
using std::set;
#include <array>
using std::array;

#include "VulkanUtilities.h"
#include "VulkanMesh.h"

struct MVP
{
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
};

class VulkanRenderer
{
public:
#ifdef NDEBUG
	static const bool enableValidationLayers = false;
#else
	static const bool enableValidationLayers = true;
#endif
	static const vector<const char*> validationLayers;

	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* windowP);
	void draw();
	void clean();

	void updateModel(glm::mat4 modelP);

private:
	GLFWwindow* window;
	vk::Instance instance;
	vk::Queue graphicsQueue;			// Handles to queue (no value stored)
	VkDebugUtilsMessengerEXT debugMessenger;
	
	struct {
		vk::PhysicalDevice physicalDevice;
		vk::Device logicalDevice;
	} mainDevice;

	vk::SurfaceKHR surface;
	vk::Queue presentationQueue;
	vk::SwapchainKHR swapchain;
	vk::Format swapchainImageFormat;
	vk::Extent2D swapchainExtent;
	vector<SwapchainImage> swapchainImages;

	vk::PipelineLayout pipelineLayout;
	vk::RenderPass renderPass;
	vk::Pipeline graphicsPipeline;

	vector<vk::Framebuffer> swapchainFramebuffers;
	vk::CommandPool graphicsCommandPool;
	vector<vk::CommandBuffer> commandBuffers;

	vector<vk::Semaphore> imageAvailable;
	vector<vk::Semaphore> renderFinished;
	const int MAX_FRAME_DRAWS = 2;			// Should be less than the number of swapchain images, here 3 (could cause bugs)
	int currentFrame = 0;
	vector<vk::Fence> drawFences;

	vector<VulkanMesh> meshes;

	vk::DescriptorSetLayout descriptorSetLayout;
	vector<vk::Buffer> uniformBuffer;
	vector<vk::DeviceMemory> uniformBufferMemory;
	vk::DescriptorPool descriptorPool;
	vector<vk::DescriptorSet> descriptorSets;
	MVP mvp;

	// Instance
	void createInstance();
	bool checkInstanceExtensionSupport(const vector<const char*>& checkExtensions);
	bool checkValidationLayerSupport();
	vector<const char*> getRequiredExtensions();

	// Debug
	void setupDebugMessenger();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	// Devices
	void getPhysicalDevice();
	bool checkDeviceSuitable(vk::PhysicalDevice device);
	QueueFamilyIndices getQueueFamilies(vk::PhysicalDevice device);
	void createLogicalDevice();

	// Surface and swapchain
	vk::SurfaceKHR createSurface();
	bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
	SwapchainDetails getSwapchainDetails(vk::PhysicalDevice device);
	void createSwapchain();
	vk::SurfaceFormatKHR chooseBestSurfaceFormat(const vector<vk::SurfaceFormatKHR>& formats);
	vk::PresentModeKHR chooseBestPresentationMode(const vector<vk::PresentModeKHR>& presentationModes);
	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities);
	vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlagBits aspectFlags);

	// Graphics pipeline
	void createGraphicsPipeline();
	vk::ShaderModule createShaderModule(const vector<char>& code);
	void createRenderPass();

	// Buffers
	void createFramebuffers();
	void createGraphicsCommandPool();
	void createGraphicsCommandBuffers();
	void recordCommands();

	// Descriptor sets
	void createDescriptorSetLayout();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
	void updateUniformBuffer(uint32_t imageIndex);

	// Draw
	void createSynchronisation();
};

