#include "VulkanRenderer.h"

const vector<const char*> VulkanRenderer::validationLayers {
	"VK_LAYER_KHRONOS_validation"
};

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* windowP)
{
	window = windowP;
	try 
	{
		// Device
		createInstance();
		setupDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();

		// Pipeline
		createSwapchain();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createDepthBufferImage();
		createFramebuffers();
		createGraphicsCommandPool();

		// Data
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();

		// Commands
		createGraphicsCommandBuffers();
		createTextureSampler();
		createSynchronisation();

		// Objects
		float aspectRatio = static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(10.0f, 10.0f, 20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		// In vulkan, y is downward, and for glm it is upward
		uboViewProjection.projection[1][1] *= -1;

		/*
		// -- Vertex data
		vector<Vertex> meshVertices1{
			{{-0.4f,  0.4f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},	// 0
			{{-0.4f, -0.4f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},	// 1
			{{ 0.4f, -0.4f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},	// 2
			{{ 0.4f,  0.4f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},	// 3
		};

		vector<Vertex> meshVertices2{
			{{-0.2f,  0.6f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},	// 0
			{{-0.2f, -0.6f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},	// 1
			{{ 0.2f, -0.6f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},	// 2
			{{ 0.2f,  0.6f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},	// 3
		};

		// -- Index data
		vector<uint32_t> meshIndices{
			0, 1, 2,
			2, 3, 0
		};

		VulkanMesh firstMesh = VulkanMesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, &meshVertices1, &meshIndices, createTexture("cat.jpg"));
		VulkanMesh secondMesh = VulkanMesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, &meshVertices2, &meshIndices, createTexture("cat.jpg"));
		meshes.push_back(firstMesh);
		meshes.push_back(secondMesh);
		*/

		// Default texture
		createTexture("cat.jpg");
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 modelP)
{
	if (modelId >= meshModels.size()) return;

	meshModels[modelId].setModel(modelP);
}

void VulkanRenderer::draw()
{
	// 0. Freeze code until the drawFences[currentFrame] is open
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint32_t>::max());
	// When passing the fence, we close it behind us
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);


	// 1. Get next available image to draw and set a semaphore to signal
	// when we're finished with the image.
	uint32_t imageToBeDrawnIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint32_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageToBeDrawnIndex);

	recordCommands(imageToBeDrawnIndex);
	updateUniformBuffers(imageToBeDrawnIndex);

	// 2. Submit command buffer to queue for execution, make sure it waits 
	// for the image to be signaled as available before drawing, and
	// signals when it has finished rendering.
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[]{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };			// Keep doing command buffer until imageAvailable is true
	submitInfo.pWaitDstStageMask = waitStages;													// Stages to check semaphores at
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageToBeDrawnIndex];							// Command buffer to submit
	submitInfo.signalSemaphoreCount = 1;													
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];								// Semaphores to signal when command buffer finishes

	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);	// When finished drawing, open the fence for the next submission
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit command buffer to queue");
	}


	// 3. Present image to screen when it has signalled finished rendering
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;															
	presentInfo.pSwapchains = &swapchain;													// Swapchains to present to
	presentInfo.pImageIndices = &imageToBeDrawnIndex;										// Index of images in swapchains to present

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present image");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::clean()
{
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	for (auto& model : meshModels)
	{
		model.destroyMeshModel();
	}
	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerDescriptorSetLayout, nullptr);
	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);
	for (auto i = 0; i < textureImages.size(); ++i)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}
	vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory, nullptr);
	_aligned_free(modelTransferSpace);
	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
		//vkDestroyBuffer(mainDevice.logicalDevice, modelUniformBufferDynamic[i], nullptr);
		//vkFreeMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[i], nullptr);
	}
	for (auto mesh : meshes)
	{
		mesh.destroyBuffers();
	}
	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i) 
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (auto image : swapchainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	if (enableValidationLayers) 
	{
		destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);	// Second argument is a custom de-allocator
}

void VulkanRenderer::createInstance()
{
	// Information about the application
	// This data is for developer convenience
	VkApplicationInfo appInfo {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;			// Type of the app info
	appInfo.pApplicationName = "Vulkan App";					// Name of the app
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);		// Version of the application
	appInfo.pEngineName = "No Engine";							// Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);			// Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_1;					// Vulkan version (here 1.1)

	// Everything we create will be created with a createInfo
	// Here, info about the vulkan creation
	VkInstanceCreateInfo createInfo {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;	// Type of the create info
	// createInfo.pNext											// Extended information
	// createInfo.flags											// Flags with bitfield
	createInfo.pApplicationInfo = &appInfo;						// Application info from above

	// Setup extensions instance will use
	vector<const char*> instanceExtensions = getRequiredExtensions();

	// Check instance extensions
	if (!checkInstanceExtensionSupport(instanceExtensions))
	{
		throw std::runtime_error("VkInstance does not support required extensions");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();
	
	// Validation layers
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (enableValidationLayers && !checkValidationLayerSupport()) 
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}
	if (enableValidationLayers) 
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else 
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	// Finally create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);	// Second argument serves to choose where to allocate memory, if you're interested with memory management
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vulkan instance");
	}
}

bool VulkanRenderer::checkInstanceExtensionSupport(const vector<const char*>& checkExtensions)
{
	// How many extensions vulkan supports
	uint32_t extensionCount = 0;
	// First get the amount of extensions
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	// Create the vector of extensions
	vector<VkExtensionProperties> extensions(extensionCount);	// A vector with a certain number of elements
	// Populate the vector
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	// This pattern, getting the number of elements then
	// populating a vector, is quite common with vulkan

	// Check if given extensions are in list of available extensions
	for (const auto& checkExtension : checkExtensions) 
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension) return false;
	}

	return true;
}

void VulkanRenderer::setupDebugMessenger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to set up debug messenger.");
	}
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
} 

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// Check if all of the layers in validation layers exist in the available layers
	for (const char* layerName : validationLayers) 
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) 
		{
			if (strcmp(layerName, layerProperties.layerName) == 0) 
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound) return false;
	}

	return true;
}

vector<const char*> VulkanRenderer::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void VulkanRenderer::getPhysicalDevice()
{
	// Get the number of devices then populate the physical device vector
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// If no devices available
	if (deviceCount == 0) 
	{
		throw std::runtime_error("Can't find any GPU that supports vulkan");
	}

	vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	// Get device valid for what we want to do
	for (const auto& device : devices)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	// Get properties of our new device to know some values
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
	minUniformBufferOffet = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	// Information about the device itself (ID, name, type, vendor, etc.)
	VkPhysicalDeviceProperties deviceProperties {};
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Information about what the device can do (geom shader, tesselation, wide lines...)
	VkPhysicalDeviceFeatures deviceFeatures {};
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// For now we do nothing with this info

	QueueFamilyIndices indices = getQueueFamilies(device);
	bool extensionSupported = checkDeviceExtensionSupport(device);

	bool swapchainValid = false;
	if (extensionSupported)
	{
		SwapchainDetails swapchainDetails = getSwapchainDetails(device);
		swapchainValid = !swapchainDetails.presentationModes.empty() && !swapchainDetails.formats.empty();
	}

	return indices.isValid() && extensionSupported && swapchainValid && deviceFeatures.samplerAnisotropy;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	// Go through each queue family and check it has at least one required type of queue
	int i = 0;
	for (const auto& queueFamily : queueFamilies) 
	{
		// Check there is at least graphics queue
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		// Check if queue family support presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.isValid()) break;
		++i;
	}
	return indices;
}

void VulkanRenderer::createLogicalDevice()
{
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation information, and set for family indices.
	// A set will only keep one indice if they are the same.
	vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };	

	// Queues the logical device needs to create and info to do so.
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		// Vulkan needs to know how to handle multiple queues. It uses priorities.
		// 1 is the highest priority.
		queueCreateInfo.pQueuePriorities = &priority;	

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Logical device creation
	VkDeviceCreateInfo deviceCreateInfo {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	// Queues info
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	// Extensions info
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());	// Device extensions, different from instance extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	// -- Validation layers are deprecated since Vulkan 1.1
	// Features
	VkPhysicalDeviceFeatures deviceFeatures {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	// Create the logical device for the given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not create the logical device.");
	}

	// Ensure access to queues
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

VkResult VulkanRenderer::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderer::destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanRenderer::createSurface()
{
	// Create a surface relatively to our window
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a vulkan surface.");
	}
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	if (extensionCount == 0)
	{
		return false;
	}
	vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension) return false;
	}

	return true;
}

SwapchainDetails VulkanRenderer::getSwapchainDetails(VkPhysicalDevice device)
{
	SwapchainDetails swapchainDetails;
	// Capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapchainDetails.surfaceCapabilities);
	// Formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		swapchainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapchainDetails.formats.data());
	}
	// Presentation modes
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);
	if (presentationCount != 0)
	{
		swapchainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapchainDetails.presentationModes.data());
	}

	return swapchainDetails;
}

void VulkanRenderer::createSwapchain()
{
	// We will pick best settings for the swapchain
	SwapchainDetails swapchainDetails = getSwapchainDetails(mainDevice.physicalDevice);
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapchainDetails.formats);
	VkPresentModeKHR presentationMode = chooseBestPresentationMode(swapchainDetails.presentationModes);
	VkExtent2D extent = chooseSwapExtent(swapchainDetails.surfaceCapabilities);

	// Setup the swap chain info
	VkSwapchainCreateInfoKHR swapchainCreateInfo {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.presentMode = presentationMode;
	swapchainCreateInfo.imageExtent = extent;
	// Minimal number of image in our swapchain. We will use one more than the minimum to enable triple-buffering.
	uint32_t imageCount = swapchainDetails.surfaceCapabilities.minImageCount + 1;
	if (swapchainDetails.surfaceCapabilities.maxImageCount > 0 // Not limitless
		&& swapchainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapchainDetails.surfaceCapabilities.maxImageCount;
	}
	swapchainCreateInfo.minImageCount = imageCount;
	// Number of layers for each image in swapchain
	swapchainCreateInfo.imageArrayLayers = 1;
	// What attachment go with the image (e.g. depth, stencil...). Here, just color.
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// Transform to perform on swapchain images
	swapchainCreateInfo.preTransform = swapchainDetails.surfaceCapabilities.currentTransform;
	// Handles blending with other windows. Here we don't blend.
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// Whether to clip parts of the image not in view (e.g. when an other window overlaps)
	swapchainCreateInfo.clipped = VK_TRUE;

	// Queue management
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	uint32_t queueFamilyIndices[]{ (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentationFamily };
	// If graphics and presentation families are different, share images between them
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else 
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}
	
	// When you want to pass old swapchain responsibilities when destroying it,
	// e.g. when you want to resize window, use this
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create swap chain
	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swapchain");
	}

	// Store for later use
	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;

	// Get the swapchain images
	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, nullptr);
	vector<VkImage> images(swapchainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, images.data());
	for (VkImage image : images)	// We are using handles, not values
	{
		SwapchainImage swapchainImage {};
		swapchainImage.image = image;

		// Create image view
		swapchainImage.imageView = createImageView(image, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		swapchainImages.push_back(swapchainImage);
	}
}

VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const vector<VkSurfaceFormatKHR>& formats)
{
	// We will use RGBA 32bits normalized and SRGG non linear colorspace
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) 
	{
		// All formats available by convention
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (auto& format : formats)
	{
		if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	// Return first format if we have not our chosen format
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const vector<VkPresentModeKHR>& presentationModes)
{
	// We will use mail box presentation mode
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	// Part of the Vulkan spec, so have to be available
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	// Rigid extents
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	// Extents can vary
	else
	{
		// Create new extent using window size
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D newExtent {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Sarface also defines max and min, so make sure we are within boundaries
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));
		return newExtent;
	}
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;					// Other formats can be used for cubemaps etc.
	viewCreateInfo.format = format;										// Can be used for depth for instance
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;		// Swizzle used to remap color values. Here we keep the same.
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;			// Here we want to see the image under the aspect of colors
	viewCreateInfo.subresourceRange.baseMipLevel = 0;					// Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;						// Number of mipmap level to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;					// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;						// Number of array levels to view

	// Create image view
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not create the image view.");
	}

	return imageView;
}

void VulkanRenderer::createGraphicsPipeline()
{
	// Read shader code and format it through a shader module
	auto vertexShaderCode = readShaderFile("shaders/vert.spv");
	auto fragmentShaderCode = readShaderFile("shaders/frag.spv");
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);


	// -- SHADER STAGE CREATION INFO --
	// Vertex stage creation info
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo{};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;		// Used to know which shader
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main";							// Pointer to the start function in the shader
	// Fragment stage creation info
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo{};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main";
	// Graphics pipeline requires an array of shader create info
	VkPipelineShaderStageCreateInfo shaderStages[]{ vertexShaderCreateInfo, fragmentShaderCreateInfo };


	// Create pipeline
	
	// Vertex description
	// -- Binding, data layout
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;									// Binding position. Can bind multiple streams of data.
	bindingDescription.stride = sizeof(Vertex);						// Size of a single vertex data object, like in OpenGL
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		// How ot move between data after each vertex.
																	// VK_VERTEX_INPUT_RATE_VERTEX: move onto next vertex
																	// VK_VERTEX_INPUT_RATE_INSTANCE: move to a vertex for the next instance. 
																	// Draw each first vertex of each instance, then the next vertex etc.
	// Different attributes
	array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

	// Position attributes
	attributeDescriptions[0].binding = 0;							// Binding of first attribute. Relate to binding description.
	attributeDescriptions[0].location = 0;							// Location in shader
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	// Format and size of the data (here: vec3)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);		// Offset of data in vertex, like in OpenGL. The offset function automatically find it.

	// Color attributes
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;	
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;	
	attributeDescriptions[1].offset = offsetof(Vertex, col);

	// Texture attributes
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, tex);

	// -- VERTEX INPUT STAGE --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();


	// -- INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		// How to assemble vertices
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;					// When you want to restart a primitive, e.g. with a strip


	// -- VIEWPORT AND SCISSOR --
	// Create a viewport info struct
	VkViewport viewport{};
	viewport.x = 0.0f;									// X start coordinate
	viewport.y = 0.0f;									// Y start coordinate
	viewport.width = (float)swapchainExtent.width;		// Width of viewport
	viewport.height = (float)swapchainExtent.height;	// Height of viewport
	viewport.minDepth = 0.0f;							// Min framebuffer depth
	viewport.maxDepth = 1.0f;							// Max framebuffer depth

	// Create a scissor info struct, everything outside is cut
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	// -- DYNAMIC STATE --
	// This will be alterable, so you don't have to create an entire pipeline when you want to change parameters.
	// We won't use this feature, this is an example.
	/*
	vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);	// Viewport can be resized in the command buffer with vkCmdSetViewport(commandBuffer, 0, 1, &newViewport);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);	// Scissors can be resized in the command buffer with vkCmdSetScissor(commandBuffer, 0, 1, &newScissor);

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
	*/


	// -- RASTERIZER --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;					// Treat elements beyond the far plane like being on the far place, needs a GPU device feature
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;			// Whether to discard data and skip rasterizer. When you want a pipeline without framebuffer.
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;			// How to handle filling points between vertices. Here, considers things inside the polygon as a fragment. VK_POLYGON_MODE_LINE will consider element inside polygones being empty (no fragment). May require a device feature.
	rasterizerCreateInfo.lineWidth = 1.0f;								// How thick should line be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;				// Culling. Do not draw back of polygons
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// Widing to know the front face of a polygon
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;					// Whether to add a depth offset to fragments. Good for stopping "shadow acne" in shadow mapping. Is set, need to set 3 other values.


	// -- MULTISAMPLING --	
	// Not for textures, only for edges
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo{};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;					// Enable multisample shading or not
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// Number of samples to use per fragment

	// -- BLENDING --		
	// How to blend a new color being written to the fragment, with the old value
	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo{};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;		// Alternative to usual blending calculation

	// Enable blending and choose colors to apply blending to
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;	
	colorBlendAttachment.blendEnable = VK_TRUE;

	// Blending equation: (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

	// Replace the old alpha with the new one: (1 * new alpha) + (0 * old alpha)
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorBlendAttachment;


	// -- PIPELINE LAYOUT --
	array<VkDescriptorSetLayout, 2> descriptorSetLayouts{ descriptorSetLayout, samplerDescriptorSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	// Create pipeline layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Pipeline Layout!");
	}


	// -- DEPTH STENCIL TESTING --
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;			// Enable checking depth
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;			// Enable writing (replace old values) to depth buffer
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;	
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;	// Does the depth value exist between two bounds?
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;		// Enable stencil test


	// -- PASSES --
	// Passes are composed of a sequence of subpasses that can pass data from one to another

	// -- GRAPHICS PIPELINE CREATION --
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStages;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = nullptr;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = renderPass;					// Renderpass description the pipeline is compatible with. This pipeline will be used by the render pass.
	graphicsPipelineCreateInfo.subpass = 0;								// Subpass of render pass to use with pipeline. Usually one pipeline by subpass.
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;		// When you want to derivate a pipeline from an other pipeline OR
	graphicsPipelineCreateInfo.basePipelineIndex = -1;					// Index of pipeline being created to derive from (in case of creating multiple at once)
		
	// Now we create ONE graphics pipeline. We coul have created more.
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline);		// The handle is a cache when you want to save your pipeline to create an other later
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Cound not create a graphics pipeline");
	}
	

	// Destroy shader modules
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

VkShaderModule VulkanRenderer::createShaderModule(const vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());		// Conversion between pointer types with reinterpret_cast

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not create shader module.");
	}
	return shaderModule;
}

void VulkanRenderer::createRenderPass()
{
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	// -- ATTACHMENTs --
	// Attachement description : describe color buffer output, depth buffer output... 
	// e.g. (location = 0) in the fragment shader is the first attachment
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchainImageFormat;						// Format to use for attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					// Number of samples t write for multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				// What to do with attachement before renderer. Here, clear when we start the render pass.
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				// What to do with attachement after renderer. Here, store the render pass.
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// What to do with stencil before renderer. Here, don't care, we don't use stencil.
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// What to do with stencil after renderer. Here, don't care, we don't use stencil.

	// Framebuffer images will be stored as an image, but image can have different layouts
	// to give optimal use for certain operations
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			// Image data layout before render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// Image data layout after render pass

	// Depth attachment of renderpass
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT,					
		VK_FORMAT_D32_SFLOAT,		
		VK_FORMAT_D24_UNORM_S8_UINT },	
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				// Clear when we start the render pass.
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;			// We do not do anything after depth buffer image is calculated
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	array<VkAttachmentDescription, 2> renderPassAttachments{ colorAttachment, depthAttachment };

	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();


	// -- REFERENCES --
	// Attachment reference uses an attachment index that refers to index in the attachement list passed to renderPassCreateInfo
	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// Layout of the subpass (between initial and final layout)

	VkAttachmentReference depthAttachmentReference{};
	depthAttachmentReference.attachment = 1;										// Second position in the order of attachment
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	// -- SUBPASSES --
	// Subpass description, will reference attachements
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;		// Pipeline type the subpass will be bound to. Could be compute pipeline, or nvidia raytracing...
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;

	// Subpass dependencies: transitions between subpasses + from the last subpass to what happens after
	// Need to determine when layout transitions occur using subpass dependencies. Will define implicitly layout transitions.
	array<VkSubpassDependency, 2> subpassDependencies;
	// -- From layout undefined to color attachment optimal
	// ---- Transition must happens after
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;										// External means from outside the subpasses
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;						// Which stage of the pipeline has to happen before
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	// ---- But must happens before
	subpassDependencies[0].dstSubpass = 0;															// Conversion should happen before the first subpass starts
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;	// ...and before the color attachment attempts to read or write
	subpassDependencies[0].dependencyFlags = 0;														// No dependency flag

	// -- From layout color attachment optimal to image layout present
	// ---- Transition must happens after
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	// ---- But must happens before
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo,nullptr, &renderPass);
	if (result != VK_SUCCESS) 
	{
		throw std::runtime_error("Could not create render pass.");
	}
}

void VulkanRenderer::createFramebuffers()
{
	// Create one framebuffer for each swapchain image
	swapchainFramebuffers.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainFramebuffers.size(); ++i)
	{
		// Setup attachments
		array<VkImageView, 2> attachments{ 
			swapchainImages[i].imageView,
			depthBufferImageView
		};

		// Create info
		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;											// Render pass layout the framebuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();								// List of attachments (1:1 with render pass, thanks to variable i)
		framebufferCreateInfo.width = swapchainExtent.width;
		framebufferCreateInfo.height = swapchainExtent.height;
		framebufferCreateInfo.layers = 1;														// Framebuffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapchainFramebuffers[i]);
		if (result != VK_SUCCESS) 
		{
			throw std::runtime_error("Failed to create a framebuffer");
		}
	}
}

void VulkanRenderer::createGraphicsCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// Command pool will be reset
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;		// Queue family type that buffers from this command pool will use

	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics command pool");
	}
;}

void VulkanRenderer::createGraphicsCommandBuffers()
{
	// Create one command buffer for each framebuffer
	commandBuffers.resize(swapchainFramebuffers.size());

	VkCommandBufferAllocateInfo commandBufferAllocInfo{};				// We are using a pool
	commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocInfo.commandPool = graphicsCommandPool;
	commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;		// Primary means the command buffer will submit directly to a queue. 
																		// Secondary cannot be called by a queue, but by an other primary command buffer, via vkCmdExecuteCommands.

	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &commandBufferAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate graphics command buffers");
	}
}

/*
void VulkanRenderer::recordCommands()
{
	// How to begin each command buffer
	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	//commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmited when it has already been submited

	// Information about how to being a render pass (only for graphical apps)
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;										// Render pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };									// Start point of render pass in pixel
	renderPassBeginInfo.renderArea.extent = swapchainExtent;							// Size of region to run render pass on
	VkClearValue clearValues[]{
		{ 0.6f, 0.65f, 0.4f, 1.0f}
	};
	renderPassBeginInfo.pClearValues = clearValues;										// List of clear vales (TODO: add depth attachment clear value)
	renderPassBeginInfo.clearValueCount = 1;

	for (size_t i = 0; i < commandBuffers.size(); ++i)
	{
		// Because 1-to-1 relationship
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];

		// Start recording commands to command buffer
		VkResult result = vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording to command buffer");
		}

		// Begin render pass
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);		// All draw commands inline (no secondary command buffers)

		// Bind pipeline to be used in render pass, you could switch pipelines for different subpasses
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Draw all meshes
		for (size_t j = 0; j < meshes.size(); ++j)
		{
			// Bind vertex buffer
			VkBuffer vertexBuffers[] = { meshes[j].getVertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);

			// Bind index buffer
			vkCmdBindIndexBuffer(commandBuffers[i], meshes[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			// Dynamic offet amount
			uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignement) * j;

			// Bind descriptor sets
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 1, &dynamicOffset);

			// Execute pipeline
			vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(meshes[j].getIndexCount()), 1, 0, 0, 0);
		}

		// End render pass
		vkCmdEndRenderPass(commandBuffers[i]);

		// Stop recordind to command buffer
		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to stop recording to command buffer");
		}
	}
}
*/

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	// How to begin each command buffer
	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	//commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmited when it has already been submited

	// Information about how to being a render pass (only for graphical apps)
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;										// Render pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };									// Start point of render pass in pixel
	renderPassBeginInfo.renderArea.extent = swapchainExtent;							// Size of region to run render pass on

	array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.6f, 0.65f, 0.4f, 1.0f };
	clearValues[1].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();								
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	// Because 1-to-1 relationship
	renderPassBeginInfo.framebuffer = swapchainFramebuffers[currentImage];

	// Start recording commands to command buffer
	VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &commandBufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording to command buffer");
	}

	// Begin render pass
	vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);		// All draw commands inline (no secondary command buffers)

	// Bind pipeline to be used in render pass, you could switch pipelines for different subpasses
	vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Draw all meshes
	for (size_t j = 0; j < meshModels.size(); ++j)
	{
		// Push constants to given shader stage
		VulkanMeshModel model = meshModels[j];
		glm::mat4 modelMatrix = model.getModel();
		vkCmdPushConstants(commandBuffers[currentImage], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model), &modelMatrix);
		// We have one model matric for each object, then several children meshes

		for(size_t k = 0; k < model.getMeshCount(); ++k)
		{
			// Bind vertex buffer
			VkBuffer vertexBuffers[] = { model.getMesh(k)->getVertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);

			// Bind index buffer
			vkCmdBindIndexBuffer(commandBuffers[currentImage], model.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			// Dynamic offet amount
			uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignement) * j;

			// Bind descriptor sets
			array<VkDescriptorSet, 2> descriptorSetsGroup{ descriptorSets[currentImage], samplerDescriptorSets[model.getMesh(k)->getTexId()] };
			vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 
				static_cast<uint32_t>(descriptorSetsGroup.size()), descriptorSetsGroup.data(), 0, nullptr);

			// Execute pipeline
			vkCmdDrawIndexed(commandBuffers[currentImage], static_cast<uint32_t>(model.getMesh(k)->getIndexCount()), 1, 0, 0, 0);
		}
	}

	// End render pass
	vkCmdEndRenderPass(commandBuffers[currentImage]);

	// Stop recordind to command buffer
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording to command buffer");
	}
}

void VulkanRenderer::createSynchronisation()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creation info
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;		// That's all !

	// Fence creation info
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;						// Fence starts open

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS
			|| vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS
			|| vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create semaphores and fences");
		}
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	// -- UNIFORM VALUES DESCRIPTOR SETS LAYOUT --
	// UboViewProjection binding information
	VkDescriptorSetLayoutBinding vpLayoutBinding;
	vpLayoutBinding.binding = 0;											// Binding number in shader
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		// Type of descriptor (uniform, dynamic uniform, samples...)
	vpLayoutBinding.descriptorCount = 1;									// Number of descriptors for binding
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// Shader stage to bind to (here: vertex shader)
	vpLayoutBinding.pImmutableSamplers = nullptr;							// For textures : can make sample data un changeable

	/*
	// Model
	VkDescriptorSetLayoutBinding modelLayoutBinding;
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;

	vector<VkDescriptorSetLayoutBinding> layoutBindings{ vpLayoutBinding, modelLayoutBinding };
	*/
	vector<VkDescriptorSetLayoutBinding> layoutBindings{ vpLayoutBinding };

	// Descriptor set layout with given binding
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutCreateInfo.pBindings = layoutBindings.data();

	// Create descriptor set layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Descriptor set layout for uniforms");
	}

	// -- SAMPLER DESCRIPTOR SETS LAYOUT --
	VkDescriptorSetLayoutBinding samplerLayoutBinding;
	samplerLayoutBinding.binding = 0;											// Binding 0 for descriptor set 1
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	vector<VkDescriptorSetLayoutBinding> samplerLayoutBindings{ samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo{};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = static_cast<uint32_t>(samplerLayoutBindings.size());
	textureLayoutCreateInfo.pBindings = samplerLayoutBindings.data();

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerDescriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Descriptor set layout for sampler");
	}
}

void VulkanRenderer::createUniformBuffers()
{
	// Buffer size will be size of view-projection
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

	// Model buffer size
	VkDeviceSize modelBufferSize = modelUniformAlignement * MAX_OBJECTS;

	// One uniform buffer for each image / each command buffer
	vpUniformBuffer.resize(swapchainImages.size());
	vpUniformBufferMemory.resize(swapchainImages.size());
	//modelUniformBufferDynamic.resize(swapchainImages.size());
	//modelUniformBufferMemoryDynamic.resize(swapchainImages.size());

	// Create uniform buffers
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vpUniformBuffer[i], &vpUniformBufferMemory[i]);

		/*
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&modelUniformBufferDynamic[i], &modelUniformBufferMemoryDynamic[i]);
		*/
	}
}

void VulkanRenderer::createDescriptorPool()
{
	// -- UNIFORM DESCRIPTOR POOL --
	// One descriptor in the pool for each image
	// View projection pool
	VkDescriptorPoolSize vpPoolSize{};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	/*
	// Model pool
	VkDescriptorPoolSize modelPoolSize{};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelUniformBufferDynamic.size());

	vector<VkDescriptorPoolSize> poolSizes{ vpPoolSize, modelPoolSize };
	*/
	vector<VkDescriptorPoolSize> poolSizes{ vpPoolSize };

	// One descriptor set that contains one descriptor
	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCreateInfo.pPoolSizes = poolSizes.data();

	// Create pool
	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}

	// -- SAMPLER DESCRIPTOR POOL --
	// Texture sampler pool
	VkDescriptorPoolSize samplerPoolSize{};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS;		// Because we create descriptor sets and  images at the same type.
														// We assume one texture by object.
	VkDescriptorPoolCreateInfo samplerPoolCreateInfo{};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;		// The maximum for this is actually very high
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a descriptor pool.");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	// One descriptor set for every image/buffer
	descriptorSets.resize(swapchainImages.size());

	// We want the same layout for the right number of descriptor sets
	vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), descriptorSetLayout);

	// Allocation from the pool
	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());	// Two binding for one set
	setAllocInfo.pSetLayouts = setLayouts.data();

	// Allocate multiple descriptor sets
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// We have a connection between descriptor set layouts and descriptor sets,
	// but we don't know how link descriptor sets and the uniform buffers.

	// Update all of descriptor set buffer bindings
	for (size_t i = 0; i < vpUniformBuffer.size(); ++i)
	{
		// -- VIEW PROJECTION DESCRIPTOR --
		// Description of the buffer and data offset
		VkDescriptorBufferInfo vpBufferInfo{};
		vpBufferInfo.buffer = vpUniformBuffer[i];		// Buffer to get data from
		vpBufferInfo.offset = 0;						// We bind the whole data
		vpBufferInfo.range = sizeof(UboViewProjection);				// Size of data

		// Data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite{};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];			// Descriptor sets to update
		vpSetWrite.dstBinding = 0;						// Binding to update (matches with shader binding)
		vpSetWrite.dstArrayElement = 0;				// Index in array to update
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpSetWrite.descriptorCount = 1;				// Amount of descriptor sets to update
		vpSetWrite.pBufferInfo = &vpBufferInfo;		// Information about buffer data to bind

		/*
		// -- MODEL DESCRIPTOR --
		// Description of the buffer and data offset
		VkDescriptorBufferInfo modelBufferInfo{};
		modelBufferInfo.buffer = modelUniformBufferDynamic[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = modelUniformAlignement;

		// Data about connection between binding and buffer
		VkWriteDescriptorSet modelSetWrite{};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.pBufferInfo = &modelBufferInfo;

		// Descriptor set writes vector
		vector<VkWriteDescriptorSet> setWrites{ vpSetWrite, modelSetWrite };
		*/
		vector<VkWriteDescriptorSet> setWrites{ vpSetWrite };

		// Update descriptor set with new buffer/binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	// Copy view projection data
	void* data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	// Copy model data (dynamic uniform buffer) NOT IN USE
	/*
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		// Get a pointer to model in modelTransferSpace
		Model* model = (Model*)((uint64_t)modelTransferSpace + i * modelUniformAlignement);
		// Copy data at this adress
		*model = meshes[i].getModel();
	}
	vkMapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex], 0, modelUniformAlignement * meshes.size(), 0, &data);
	memcpy(data, modelTransferSpace, modelUniformAlignement * meshes.size());
	vkUnmapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex]);
	*/
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	// modelUniformAlignement = sizeof(Model) & ~(minUniformBufferOffet - 1);

	// We take the size of Model and we compare its size to a mask.
	// ~(minUniformBufferOffet - 1) is the inverse of minUniformBufferOffet
	// Example with a 16bits alignment coded on 8 bits:
	//   00010000 - 1  == 00001111
	// ~(00010000 - 1) == 11110000 which is our mask.
	// If we imagine our Model is 64 bits (01000000) 
	// and the minUniformBufferOffet 16 bits (00010000), 
	// (01000000) & ~(00010000 - 1) == 01000000 & 11110000 == 01000000
	// Our alignment will need to be 64 bits.

	// However this calculation is not perfect.

	// Let's now imagine our Model is 66 bits : 01000010.
	// The above calculation would give us a 64 bits alignment,
	// whereas we would need a 80 bits (01010000 = 64 + 16) alignment.

	// We need to add to the size minUniformBufferOffet - 1 to shield against this effect
	modelUniformAlignement = (sizeof(Model) + minUniformBufferOffet - 1) & ~(minUniformBufferOffet - 1);

	// Example:
	// Model size is 01000010. If we add minUniformBufferOffet - 1 (00001111) to the size,
	// we obtain 01010001. With a 11110000 mask, we get a 01010000 == 80 bits alignement.

	// We will now allocate memory for models on dynamic buffer
	modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignement * MAX_OBJECTS, modelUniformAlignement);
}

void VulkanRenderer::createPushConstantRange()
{
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;		// Shader stage push constant will go to
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Model);
}

void VulkanRenderer::createDepthBufferImage()
{
	VkFormat depthFormat = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT,					// Look for a format with 32bits death buffer and stencil buffer,
		VK_FORMAT_D32_SFLOAT,							// if not found, without stencil
		VK_FORMAT_D24_UNORM_S8_UINT },					// if not 24bits depth and stencil buffer
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT	// Format supports depth and stencil attachment														
	);

	// Create image and image view
	depthBufferImage = createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory);

	depthBufferImageView = createImageView(depthBufferImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;								// 1, no 3D aspect
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;								// Number of levels in image array
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;								// How image data should be "tiled" (arranged for optimal reading)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// Initial layout in the render pass
	imageCreateInfo.usage = useFlags;								// Bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;				// Number of samples for multi sampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Whether image can be shared between queues (no)

	// Create the header of the image
	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image.");
	}

	// Now we need to setup and allocate memory for the image
	VkMemoryRequirements memoryRequierements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequierements);

	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequierements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequierements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for an image.");
	}

	// Connect memory to image
	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

VkFormat VulkanRenderer::chooseSupportedFormat(const vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	// Loop through the options and find a compatible format
	for (VkFormat format : formats)
	{
		// Get properties for a given format on this device
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		// If the tiling is linear and all feature flags match
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		// If the tiling is optimal and all feature flags match
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}
	throw std::runtime_error("Failed to find a matching format.");
}

stbi_uc* VulkanRenderer::loadTextureFile(string filename, int* width, int* height, VkDeviceSize* imageSize)
{
	// Number of channel image uses
	int channels;

	// Load pixel data for image
	string path = "textures/" + filename;
	stbi_uc* image = stbi_load(path.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image) 
	{
		throw std::runtime_error("Failed to load texture file: " + path);
	}

	*imageSize = *width * *height * 4;	// RGBA has 4 channels
	return image;
}

int VulkanRenderer::createTextureImage(string filename)
{
	// Load image file
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc* imageData = loadTextureFile(filename, &width, &height, &imageSize);

	// Create staging buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&imageStagingBuffer, &imageStagingBufferMemory);

	// Copy image data to the staging buffer
	void* data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	// Free original image data
	stbi_image_free(imageData);

	// Create image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;
	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

	// -- COPY DATA TO IMAGE --
	// Transition image to be DST for copy operations
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// Copy image data
	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);

	// -- READY FOR SHADER USE --
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Add texture data to vector for reference
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy stagin buffers
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	// Return index of new texture image
	return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(string filename)
{
	int textureImageLocation = createTextureImage(filename);

	VkImageView imageView = createImageView(textureImages[textureImageLocation], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	int descriptorLoc = createTextureDescriptor(imageView);

	// Return location of set with texture
	return descriptorLoc;
}

void VulkanRenderer::createTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;						// How to render when image is magnified on screen
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;						// How to render when image is minified on screen
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// Texture wrap in the U direction
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// Texture wrap in the V direction
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// Texture wrap in the W direction
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	// When no repeat, texture become black beyond border
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;				// Coordinates ARE normalized. When true, coords are between 0 and image size
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;		// Fade between two mipmaps is linear
	samplerCreateInfo.mipLodBias = 0.0f;								// Add a bias to the mimmap level
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;						// Overcome blur when a texture is stretched because of perspective with angle
	samplerCreateInfo.maxAnisotropy = 16;								// Anisotropy number of samples

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler");
	}
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImageView)
{
	VkDescriptorSet descriptorSet;

	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerDescriptorSetLayout;

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture descriptor set.");
	}

	// Texture image info
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;		// Image layout when in use
	imageInfo.imageView = textureImageView;									// Image view to bind to set
	imageInfo.sampler = textureSampler;										// Sampler to use for set

	// Write info
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	return samplerDescriptorSets.size() - 1;
}

int VulkanRenderer::createMeshModel(string filename)
{
	// Import model scene
	Assimp::Importer importer;
	// We want the model to be in triangles, to flip vertically texels uvs, and optimize the use of vertices
	const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	if (!scene)
	{
		throw std::runtime_error("Failed to load mesh model: " + filename);
	}	

	// Load materials with one to one relationship with texture ids
	vector<string> textureNames = VulkanMeshModel::loadMaterials(scene);
	
	// Conversion to material list ID to descriptor array ids (we don't keep empty files)
	vector<int> matToTex(textureNames.size());
	// Loop over texture names and create textures for them
	for (size_t i = 0; i < textureNames.size(); ++i)
	{
		if (textureNames[i].empty())
		{
			// Texture 0 will be reserved for a default texture
			matToTex[i] = 0;
		}
		else
		{
			// Return the texture's id
			matToTex[i] = createTexture(textureNames[i]);
		}
	}

	// Load in all our meshes
	vector<VulkanMesh> modelMeshes = VulkanMeshModel::loadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, 
		graphicsQueue, graphicsCommandPool, scene->mRootNode, scene, matToTex);

	auto meshModel = VulkanMeshModel(modelMeshes);
	meshModels.push_back(meshModel);

	return meshModels.size() - 1;
}

