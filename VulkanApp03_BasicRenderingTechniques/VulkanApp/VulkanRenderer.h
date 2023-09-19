#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stdexcept>
#include <vector>
using std::vector;
#include <set>
using std::set;
#include <array>
using std::array;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"


#include "VulkanUtilities.h"
#include "VulkanMesh.h"
#include "VulkanMeshModel.h"

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
	void updateModel(int modelId, glm::mat4 modelP);
	void draw();
	void clean();

	int createMeshModel(string filename);

private:
	GLFWwindow* window;
	VkInstance instance;
	VkQueue graphicsQueue;			// Handles to queue (no value stored)
	VkDebugUtilsMessengerEXT debugMessenger;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkSurfaceKHR surface;
	VkQueue presentationQueue;
	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	vector<SwapchainImage> swapchainImages;

	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
	VkPipeline graphicsPipeline;

	vector<VkFramebuffer> swapchainFramebuffers;
	VkCommandPool graphicsCommandPool;
	vector<VkCommandBuffer> commandBuffers;

	vector<VkSemaphore> imageAvailable;
	vector<VkSemaphore> renderFinished;
	const int MAX_FRAME_DRAWS = 2;			// Should be less than the number of swapchain images, here 3 (could cause bugs)
	int currentFrame = 0;
	vector<VkFence> drawFences;

	const int MAX_OBJECTS = 20;

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
	bool checkDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	void createLogicalDevice();

	// Surface and swapchain
	void createSurface();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	SwapchainDetails getSwapchainDetails(VkPhysicalDevice device);
	void createSwapchain();
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentationMode(const vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	// Graphics pipeline
	void createGraphicsPipeline();
	VkShaderModule createShaderModule(const vector<char>& code);
	void createRenderPass();

	// Buffers
	void createFramebuffers();
	void createGraphicsCommandPool();
	void createGraphicsCommandBuffers();
	void recordCommands(uint32_t currentImage);

	// Draw
	void createSynchronisation();

	// Objects
	vector<VulkanMesh> meshes;

	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;

	// Descriptors: uniform buffer
	VkDescriptorSetLayout descriptorSetLayout;
	void createDescriptorSetLayout();
	vector<VkBuffer> vpUniformBuffer;
	vector<VkDeviceMemory> vpUniformBufferMemory;
	void createUniformBuffers();
	VkDescriptorPool descriptorPool;
	void createDescriptorPool();
	vector<VkDescriptorSet> descriptorSets;
	void createDescriptorSets();
	void updateUniformBuffers(uint32_t imageIndex);

	// Descriptors: uniform buffer, dynamic uniform buffer and push constant
	VkDeviceSize minUniformBufferOffet;
	size_t modelUniformAlignement;
	Model* modelTransferSpace;
	void allocateDynamicBufferTransferSpace();
	vector<VkBuffer> modelUniformBufferDynamic;
	vector<VkDeviceMemory> modelUniformBufferMemoryDynamic;
	VkPushConstantRange pushConstantRange;
	void createPushConstantRange();

	// Depth
	VkImage depthBufferImage;
	VkDeviceMemory depthBufferImageMemory;
	VkImageView depthBufferImageView;
	void createDepthBufferImage();
	VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory);
	VkFormat chooseSupportedFormat(const vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

	// Textures
	stbi_uc* loadTextureFile(string filename, int* width, int* height, VkDeviceSize* imageSize);
	int createTextureImage(string filename);
	vector<VkImage> textureImages;
	vector<VkDeviceMemory> textureImageMemory;
	vector<VkImageView> textureImageViews;
	int createTexture(string filename);
	VkSampler textureSampler;
	void createTextureSampler();
	VkDescriptorPool samplerDescriptorPool;
	VkDescriptorSetLayout samplerDescriptorSetLayout;
	vector<VkDescriptorSet> samplerDescriptorSets;
	int createTextureDescriptor(VkImageView textureImageView);

	// Mesh models
	vector<VulkanMeshModel> meshModels;
};