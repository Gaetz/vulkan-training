#include "VulkanRenderer.h"

const vector<const char*> VulkanRenderer::validationLayers{
		"VK_LAYER_KHRONOS_validation" };

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
		createInstance();
		setupDebugMessenger();
		surface = createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createColorBufferImage();
		createDepthBufferImage();
		createFramebuffers();
		createGraphicsCommandPool();

		// Data
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();

		// Commands
		createGraphicsCommandBuffers();
		createTextureSampler();
		createSynchronisation();

		// Objects
		float aspectRatio = static_cast<float>(swapchainExtent.width) /
			static_cast<float>(swapchainExtent.height);
		viewProjection.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
		viewProjection.view = glm::lookAt(glm::vec3(10.0f, 10.0f, 20.0f),
			glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		// In vulkan, y is downward, and for glm it is upward
		viewProjection.projection[1][1] *= -1;

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

void VulkanRenderer::draw()
{
	// 0. Freeze code until the drawFences[currentFrame] is open
	mainDevice.logicalDevice.waitForFences(drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint32_t>::max());
	// When passing the fence, we close it behind us
	mainDevice.logicalDevice.resetFences(drawFences[currentFrame]);

	// 1. Get next available image to draw and set a semaphore to signal
	// when we're finished with the image.
	uint32_t imageToBeDrawnIndex = (mainDevice.logicalDevice.acquireNextImageKHR(swapchain,
		std::numeric_limits<uint32_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE))
		.value;

	recordCommands(imageToBeDrawnIndex);
	updateUniformBuffers(imageToBeDrawnIndex);

	// 2. Submit command buffer to queue for execution, make sure it waits
	// for the image to be signaled as available before drawing, and
	// signals when it has finished rendering.
	vk::SubmitInfo submitInfo{};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	// Keep doing command buffer until imageAvailable is true
	vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
	// Stages to check semaphores at
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	// Command buffer to submit
	submitInfo.pCommandBuffers = &commandBuffers[imageToBeDrawnIndex];
	// Semaphores to signal when command buffer finishes
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];

	// When finished drawing, open the fence for the next submission
	graphicsQueue.submit(submitInfo, drawFences[currentFrame]);

	// 3. Present image to screen when it has signalled finished rendering
	vk::PresentInfoKHR presentInfo{};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	// Swapchains to present to
	presentInfo.pSwapchains = &swapchain;
	// Index of images in swapchains to present
	presentInfo.pImageIndices = &imageToBeDrawnIndex;

	presentationQueue.presentKHR(presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::clean()
{
	mainDevice.logicalDevice.waitIdle();

	mainDevice.logicalDevice.destroyImageView(colorImageView);
	mainDevice.logicalDevice.destroyImage(colorImage);
	mainDevice.logicalDevice.freeMemory(colorImageMemory);
	for (auto& model : meshModels)
	{
		model.destroyMeshModel();
	}
	mainDevice.logicalDevice.destroyDescriptorPool(samplerDescriptorPool, nullptr);
	mainDevice.logicalDevice.destroyDescriptorSetLayout(samplerDescriptorSetLayout, nullptr);
	mainDevice.logicalDevice.destroySampler(textureSampler);
	for (auto i = 0; i < textureImages.size(); ++i)
	{
		mainDevice.logicalDevice.destroyImageView(textureImageViews[i], nullptr);
		mainDevice.logicalDevice.destroyImage(textureImages[i], nullptr);
		mainDevice.logicalDevice.freeMemory(textureImageMemory[i], nullptr);
	}
	mainDevice.logicalDevice.destroyImageView(depthBufferImageView);
	mainDevice.logicalDevice.destroyImage(depthBufferImage);
	mainDevice.logicalDevice.freeMemory(depthBufferImageMemory);
	mainDevice.logicalDevice.destroyDescriptorPool(descriptorPool);
	mainDevice.logicalDevice.destroyDescriptorSetLayout(descriptorSetLayout);
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		vpUniformBuffer[i].destroy();
	}
	for (auto& mesh : meshes)
	{
		mesh.destroyBuffers();
	}
	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		mainDevice.logicalDevice.destroySemaphore(renderFinished[i]);
		mainDevice.logicalDevice.destroySemaphore(imageAvailable[i]);
		mainDevice.logicalDevice.destroyFence(drawFences[i]);
	}
	mainDevice.logicalDevice.destroyCommandPool(graphicsCommandPool);
	for (auto framebuffer : swapchainFramebuffers)
	{
		mainDevice.logicalDevice.destroyFramebuffer(framebuffer);
	}
	mainDevice.logicalDevice.destroyPipeline(graphicsPipeline);
	mainDevice.logicalDevice.destroyPipelineLayout(pipelineLayout);
	mainDevice.logicalDevice.destroyRenderPass(renderPass);
	for (auto image : swapchainImages)
	{
		mainDevice.logicalDevice.destroyImageView(image.imageView);
	}
	mainDevice.logicalDevice.destroySwapchainKHR(swapchain);
	instance.destroySurfaceKHR(surface);
	if (enableValidationLayers)
	{
		destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	mainDevice.logicalDevice.destroy();
	instance.destroy();
}

void VulkanRenderer::createInstance()
{
	// Information about the application
	// This data is for developer convenience
	vk::ApplicationInfo appInfo{};
	appInfo.pApplicationName = "Vulkan App";							 // Name of the app
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Version of the application
	appInfo.pEngineName = "No Engine";										 // Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);			 // Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_1;							 // Vulkan version (here 1.1)

	// Everything we create will be created with a createInfo
	// Here, info about the vulkan creation
	vk::InstanceCreateInfo createInfo{};
	// createInfo.pNext											// Extended information
	// createInfo.flags											// Flags with bitfield
	createInfo.pApplicationInfo = &appInfo; // Application info from above

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
	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
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
	instance = vk::createInstance(createInfo);
}

bool VulkanRenderer::checkInstanceExtensionSupport(const vector<const char*>& checkExtensions)
{
	// Create the vector of extensions
	vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();

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
		if (!hasExtension)
			return false;
	}

	return true;
}

void VulkanRenderer::setupDebugMessenger()
{
	if (!enableValidationLayers)
		return;

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
	vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

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

		if (!layerFound)
			return false;
	}

	return true;
}

vector<const char*> VulkanRenderer::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void VulkanRenderer::getPhysicalDevice()
{
	// Get available physical device
	vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();

	// If no devices available
	if (devices.size() == 0)
	{
		throw std::runtime_error("Can't find any GPU that supports vulkan");
	}

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
	vk::PhysicalDeviceProperties deviceProperties = mainDevice.physicalDevice.getProperties();
	minUniformBufferOffet = deviceProperties.limits.minUniformBufferOffsetAlignment;
	
	vk::SampleCountFlags counts = deviceProperties.limits.framebufferColorSampleCounts & deviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & vk::SampleCountFlagBits::e64) msaaSamples = vk::SampleCountFlagBits::e64;
	else if (counts & vk::SampleCountFlagBits::e32) msaaSamples = vk::SampleCountFlagBits::e32;
	else if (counts & vk::SampleCountFlagBits::e16) msaaSamples = vk::SampleCountFlagBits::e16;
	else if (counts & vk::SampleCountFlagBits::e8) msaaSamples = vk::SampleCountFlagBits::e8;
	else if (counts & vk::SampleCountFlagBits::e4) msaaSamples = vk::SampleCountFlagBits::e4;
	else if (counts & vk::SampleCountFlagBits::e2) msaaSamples = vk::SampleCountFlagBits::e2;
	else msaaSamples = vk::SampleCountFlagBits::e1;
}

bool VulkanRenderer::checkDeviceSuitable(vk::PhysicalDevice device)
{
	// Information about the device itself (ID, name, type, vendor, etc.)
	vk::PhysicalDeviceProperties deviceProperties = device.getProperties();

	// Information about what the device can do (geom shader, tesselation, wide lines...)
	vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();

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

QueueFamilyIndices VulkanRenderer::getQueueFamilies(vk::PhysicalDevice device)
{
	QueueFamilyIndices indices;
	vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

	// Go through each queue family and check it has at least one required type of queue
	int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		// Check there is at least graphics queue
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
		{
			indices.graphicsFamily = i;
		}

		// Check if queue family support presentation
		VkBool32 presentationSupport = false;
		presentationSupport = device.getSurfaceSupportKHR(static_cast<uint32_t>(indices.graphicsFamily), surface);
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.isValid())
			break;
		++i;
	}
	return indices;
}

void VulkanRenderer::createLogicalDevice()
{
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation information, and set for family indices.
	// A set will only keep one indice if they are the same.
	vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

	// Queues the logical device needs to create and info to do so.
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		vk::DeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		// Vulkan needs to know how to handle multiple queues. It uses priorities.
		// 1 is the highest priority.
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Logical device creation
	vk::DeviceCreateInfo deviceCreateInfo{};
	// Queues info
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	// Extensions info
	// Device extensions, different from instance extensions
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Features
	vk::PhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = true;
	deviceFeatures.sampleRateShading = true;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	// Create the logical device for the given physical device
	mainDevice.logicalDevice = mainDevice.physicalDevice.createDevice(deviceCreateInfo);

	// Ensure access to queues
	graphicsQueue = mainDevice.logicalDevice.getQueue(indices.graphicsFamily, 0);
	presentationQueue = mainDevice.logicalDevice.getQueue(indices.presentationFamily, 0);
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

vk::SurfaceKHR VulkanRenderer::createSurface()
{
	// Create a surface relatively to our window
	VkSurfaceKHR _surface;

	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &_surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a vulkan surface.");
	}

	return vk::SurfaceKHR(_surface);
}

bool VulkanRenderer::checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
	vector<vk::ExtensionProperties> extensions = device.enumerateDeviceExtensionProperties();

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

		if (!hasExtension)
			return false;
	}

	return true;
}

SwapchainDetails VulkanRenderer::getSwapchainDetails(vk::PhysicalDevice device)
{
	SwapchainDetails swapchainDetails;
	// Capabilities
	swapchainDetails.surfaceCapabilities = device.getSurfaceCapabilitiesKHR(surface);
	// Formats
	swapchainDetails.formats = device.getSurfaceFormatsKHR(surface);
	// Presentation modes
	swapchainDetails.presentationModes = device.getSurfacePresentModesKHR(surface);

	return swapchainDetails;
}

void VulkanRenderer::createSwapchain()
{
	// We will pick best settings for the swapchain
	SwapchainDetails swapchainDetails = getSwapchainDetails(mainDevice.physicalDevice);
	vk::SurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapchainDetails.formats);
	vk::PresentModeKHR presentationMode = chooseBestPresentationMode(swapchainDetails.presentationModes);
	VkExtent2D extent = chooseSwapExtent(swapchainDetails.surfaceCapabilities);

	// Setup the swap chain info
	vk::SwapchainCreateInfoKHR swapchainCreateInfo{};
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
	swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	// Transform to perform on swapchain images
	swapchainCreateInfo.preTransform = swapchainDetails.surfaceCapabilities.currentTransform;
	// Handles blending with other windows. Here we don't blend.
	swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	// Whether to clip parts of the image not in view (e.g. when an other window overlaps)
	swapchainCreateInfo.clipped = VK_TRUE;

	// Queue management
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	uint32_t queueFamilyIndices[]{ (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentationFamily };
	// If graphics and presentation families are different, share images between them
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		swapchainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	// When you want to pass old swapchain responsibilities when destroying it,
	// e.g. when you want to resize window, use this
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create swapchain
	swapchain = mainDevice.logicalDevice.createSwapchainKHR(swapchainCreateInfo);

	// Store for later use
	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;

	// Get the swapchain images
	vector<vk::Image> images = mainDevice.logicalDevice.getSwapchainImagesKHR(swapchain);
	for (VkImage image : images) // We are using handles, not values
	{
		SwapchainImage swapchainImage{};
		swapchainImage.image = image;

		// Create image view
		swapchainImage.imageView = createImageView(image, swapchainImageFormat, vk::ImageAspectFlagBits::eColor, 1);

		swapchainImages.push_back(swapchainImage);
	}
}

vk::SurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const vector<vk::SurfaceFormatKHR>& formats)
{
	// We will use RGBA 32bits normalized and SRGG non linear colorspace
	if (formats.size() == 1 && formats[0].format == vk::Format::eUndefined)
	{
		// All formats available by convention
		return { vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	}

	for (auto& format : formats)
	{
		if (format.format == vk::Format::eR8G8B8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			return format;
		}
	}

	// Return first format if we have not our chosen format
	return formats[0];
}

vk::PresentModeKHR VulkanRenderer::chooseBestPresentationMode(const vector<vk::PresentModeKHR>& presentationModes)
{
	// We will use mail box presentation mode
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == vk::PresentModeKHR::eMailbox)
		{
			return presentationMode;
		}
	}

	// Part of the Vulkan spec, so have to be available
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanRenderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities)
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
		vk::Extent2D newExtent{};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Sarface also defines max and min, so make sure we are within boundaries
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));
		return newExtent;
	}
}

vk::ImageView VulkanRenderer::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlagBits aspectFlags, uint32_t mipLevels)
{
	vk::ImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = vk::ImageViewType::e2D;				// Other formats can be used for cubemaps etc.
	viewCreateInfo.format = format;									// Can be used for depth for instance
	viewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;	// Swizzle used to remap color values. Here we keep the same.
	viewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
	viewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
	viewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;

	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;	// Here we want to see the image under the aspect of colors
	viewCreateInfo.subresourceRange.baseMipLevel = 0;			// Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = mipLevels;		// Number of mipmap level to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;			// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;				// Number of array levels to view

	// Create image view
	vk::ImageView imageView = mainDevice.logicalDevice.createImageView(viewCreateInfo);
	return imageView;
}

void VulkanRenderer::createGraphicsPipeline()
{
	// Read shader code and format it through a shader module
	auto vertexShaderCode = readShaderFile("shaders/shader1.vert.spv");
	auto fragmentShaderCode = readShaderFile("shaders/shader1.frag.spv");
	vk::ShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	vk::ShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// -- SHADER STAGE CREATION INFO --
	// Vertex stage creation info
	vk::PipelineShaderStageCreateInfo vertexShaderCreateInfo{};
	vertexShaderCreateInfo.stage = vk::ShaderStageFlagBits::eVertex; // Used to know which shader
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main"; // Pointer to the start function in the shader
	// Fragment stage creation info
	vk::PipelineShaderStageCreateInfo fragmentShaderCreateInfo{};
	fragmentShaderCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main";
	// Graphics pipeline requires an array of shader create info
	vk::PipelineShaderStageCreateInfo shaderStages[]{ vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// Create pipeline

	// Vertex description
	// -- Binding, data layout
	vk::VertexInputBindingDescription bindingDescription{};
	// Binding position. Can bind multiple streams of data.
	bindingDescription.binding = 0;
	// Size of a single vertex data object, like in OpenGL
	bindingDescription.stride = sizeof(Vertex);
	// How ot move between data after each vertex.
	// vk::VertexInputRate::eVertex: move onto next vertex
	// vk::VertexInputRate::eInstance: move to a vertex for the next instance.
	// Draw each first vertex of each instance, then the next vertex etc.
	bindingDescription.inputRate = vk::VertexInputRate::eVertex;

	// Different attributes
	array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

	// Position attributes
	// -- Binding of first attribute. Relate to binding description.
	attributeDescriptions[0].binding = 0;
	// Location in shader
	attributeDescriptions[0].location = 0;
	// Format and size of the data(here: vec3)
	attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
	// Offset of data in vertex, like in OpenGL. The offset function automatically find it.
	attributeDescriptions[0].offset = offsetof(Vertex, pos);

	// Color attributes
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
	attributeDescriptions[1].offset = offsetof(Vertex, col);

	// Texture attributes
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
	attributeDescriptions[2].offset = offsetof(Vertex, tex);

	// -- VERTEX INPUT STAGE --
	vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// -- INPUT ASSEMBLY --
	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	// How to assemble vertices
	inputAssemblyCreateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	// When you want to restart a primitive, e.g. with a strip
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	// -- VIEWPORT AND SCISSOR --
	// Create a viewport info struct
	vk::Viewport viewport{};
	viewport.x = 0.0f;															 // X start coordinate
	viewport.y = 0.0f;															 // Y start coordinate
	viewport.width = (float)swapchainExtent.width;	 // Width of viewport
	viewport.height = (float)swapchainExtent.height; // Height of viewport
	viewport.minDepth = 0.0f;												 // Min framebuffer depth
	viewport.maxDepth = 1.0f;												 // Max framebuffer depth

	// Create a scissor info struct, everything outside is cut
	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = swapchainExtent;

	vk::PipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	// -- DYNAMIC STATE --
	// This will be alterable, so you don't have to create an entire pipeline when you want to change parameters.
	// We won't use this feature, this is an example.
	/*
	vector<vk::DynamicState> dynamicStateEnables;
	// Viewport can be resized in the command buffer with vkCmdSetViewport(commandBuffer, 0, 1, &newViewport);
	dynamicStateEnables.push_back(vk::DynamicState::eViewport);
	// Scissors can be resized in the command buffer with vkCmdSetScissor(commandBuffer, 0, 1, &newScissor);
	dynamicStateEnables.push_back(vk::DynamicState::eScissor);

	vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();
	*/

	// -- RASTERIZER --
	vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
	// Treat elements beyond the far plane like being on the far place, needs a GPU device feature
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	// Whether to discard data and skip rasterizer. When you want a pipeline without framebuffer.
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	// How to handle filling points between vertices. Here, considers things inside the polygon as a fragment.
	// VK_POLYGON_MODE_LINE will consider element inside polygones being empty (no fragment). May require
	// a device feature.
	rasterizerCreateInfo.polygonMode = vk::PolygonMode::eFill;
	// How thick should line be when drawn
	rasterizerCreateInfo.lineWidth = 1.0f;
	// Culling. Do not draw back of polygons
	rasterizerCreateInfo.cullMode = vk::CullModeFlagBits::eBack;
	// Widing to know the front face of a polygon
	rasterizerCreateInfo.frontFace = vk::FrontFace::eCounterClockwise;
	// Whether to add a depth offset to fragments. Good for stopping "shadow acne" in shadow mapping.
	// Is set, need to set 3 other values.
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

	// -- MULTISAMPLING --
	// Not for textures, only for edges
	vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo{};
	// Enable multisample shading or not
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	// Number of samples to use per fragment
	multisamplingCreateInfo.rasterizationSamples = msaaSamples;
	// Enable sample shading in the pipeline
	multisamplingCreateInfo.sampleShadingEnable = true;
	// Min fraction for sample shading; closer to one is smoother
	multisamplingCreateInfo.minSampleShading = 0.2f;

	// -- BLENDING --
	// How to blend a new color being written to the fragment, with the old value
	vk::PipelineColorBlendStateCreateInfo colorBlendingCreateInfo{};
	// Alternative to usual blending calculation
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;

	// Enable blending and choose colors to apply blending to
	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_TRUE;

	// Blending equation: (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;

	// Replace the old alpha with the new one: (1 * new alpha) + (0 * old alpha)
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorBlendAttachment;

	// -- PIPELINE LAYOUT --
	array<vk::DescriptorSetLayout, 2> descriptorSetLayouts{
			descriptorSetLayout, samplerDescriptorSetLayout };

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	// Create pipeline layout
	pipelineLayout = mainDevice.logicalDevice.createPipelineLayout(pipelineLayoutCreateInfo);

	// -- DEPTH STENCIL TESTING --
	vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
	// Enable checking depth
	depthStencilCreateInfo.depthTestEnable = true;
	// Enable writing (replace old values) to depth buffer
	depthStencilCreateInfo.depthWriteEnable = true;
	depthStencilCreateInfo.depthCompareOp = vk::CompareOp::eLess;
	// Does the depth value exist between two bounds?
	depthStencilCreateInfo.depthBoundsTestEnable = false;
	// Enable stencil test
	depthStencilCreateInfo.stencilTestEnable = false;

	// -- PASSES --
	// Passes are composed of a sequence of subpasses that can pass data from one to another

	// -- GRAPHICS PIPELINE CREATION --
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
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
	// Renderpass description the pipeline is compatible with. This pipeline will be used by the render pass.
	graphicsPipelineCreateInfo.renderPass = renderPass;
	// Subpass of render pass to use with pipeline. Usually one pipeline by subpass.
	graphicsPipelineCreateInfo.subpass = 0;
	// When you want to derivate a pipeline from an other pipeline OR
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	// Index of pipeline being created to derive from (in case of creating multiple at once)
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	// The handle is a cache when you want to save your pipeline to create an other later
	auto result = mainDevice.logicalDevice.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);
	// We could have used createGraphicsPipelines to create multiple pipelines at once.
	if (result.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Cound not create a graphics pipeline");
	}
	graphicsPipeline = result.value;

	// Destroy shader modules
	mainDevice.logicalDevice.destroyShaderModule(fragmentShaderModule);
	mainDevice.logicalDevice.destroyShaderModule(vertexShaderModule);
}

vk::ShaderModule VulkanRenderer::createShaderModule(const vector<char>& code)
{
	vk::ShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.codeSize = code.size();
	// Conversion between pointer types with reinterpret_cast
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	vk::ShaderModule shaderModule = mainDevice.logicalDevice.createShaderModule(shaderModuleCreateInfo);
	return shaderModule;
}

void VulkanRenderer::createRenderPass()
{
	vk::RenderPassCreateInfo renderPassCreateInfo{};

	// Attachement description : describe color buffer output, depth buffer output...
	// e.g. (location = 0) in the fragment shader is the first attachment
	vk::AttachmentDescription colorAttachment{};
	// Format to use for attachment
	colorAttachment.format = swapchainImageFormat;
	// Number of samples to write for multisampling
	colorAttachment.samples = msaaSamples;
	// What to do with attachement before renderer. Here, clear when we start the render pass.
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	// What to do with attachement after renderer. Here, store the render pass.
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	// What to do with stencil before renderer. Here, don't care, we don't use stencil.
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	// What to do with stencil after renderer. Here, don't care, we don't use stencil.
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

	// Framebuffer images will be stored as an image, but image can have different layouts
	// to give optimal use for certain operations

	// Image data layout before render pass starts
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	// Image data layout after render pass
	colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

	// Depth attachment of renderpass
	vk::AttachmentDescription depthAttachment{};

	std::vector<vk::Format> formats{
			vk::Format::eD32SfloatS8Uint,
			vk::Format::eD32Sfloat,
			vk::Format::eD24UnormS8Uint };
	depthAttachment.format = chooseSupportedFormat(formats, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
	depthAttachment.samples = msaaSamples;
	// Clear when we start the render pass.
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	// We do not do anything after depth buffer image is calculated
	depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
	depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	// Color resolve attachment
	vk::AttachmentDescription colorAttachmentResolve{};
	colorAttachmentResolve.format = swapchainImageFormat;
	colorAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
	colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachmentResolve.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	array<vk::AttachmentDescription, 3> renderPassAttachments{
			colorAttachment, depthAttachment, colorAttachmentResolve };
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();

	// -- REFERENCES --
	// Attachment reference uses an attachment index that refers to index
	// in the attachement list passed to renderPassCreateInfo
	vk::AttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	// Layout of the subpass (between initial and final layout)
	colorAttachmentReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depthAttachmentReference{};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::AttachmentReference colorAttachmentResolveReference{};
	colorAttachmentResolveReference.attachment = 2;
	colorAttachmentResolveReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

	// -- SUBPASSES --
	// Subpass description, will reference attachements
	vk::SubpassDescription subpass{};
	// Pipeline type the subpass will be bound to.
	// Could be compute pipeline, or nvidia raytracing...
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;
	subpass.pResolveAttachments = &colorAttachmentResolveReference;

	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;

	// Subpass dependencies: transitions between subpasses + from the last subpass to what happens after
	// Need to determine when layout transitions occur using subpass dependencies.
	// Will define implicitly layout transitions.
	array<vk::SubpassDependency, 2> subpassDependencies;
	// -- From layout undefined to color attachment optimal
	// ---- Transition must happens after
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL; // External means from outside the subpasses
	// Which stage of the pipeline has to happen before
	subpassDependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	subpassDependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	// ---- But must happens before
	subpassDependencies[0].dstSubpass = 0; // Conversion should happen before the first subpass starts
	subpassDependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	// ...and before the color attachment attempts to read or write
	subpassDependencies[0].dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	subpassDependencies[0].dependencyFlags = vk::DependencyFlags(); // No dependency flag

	// -- From layout color attachment optimal to image layout present
	// ---- Transition must happens after
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	subpassDependencies[1].srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	// ---- But must happens before
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	subpassDependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	subpassDependencies[1].dependencyFlags = vk::DependencyFlags();

	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	renderPass = mainDevice.logicalDevice.createRenderPass(renderPassCreateInfo);
}

void VulkanRenderer::createFramebuffers()
{
	// Create one framebuffer for each swapchain image
	swapchainFramebuffers.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainFramebuffers.size(); ++i)
	{
		// Setup attachments
		array<vk::ImageView, 3> attachments{ colorImageView, depthBufferImageView, swapchainImages[i].imageView };

		// Create info
		vk::FramebufferCreateInfo framebufferCreateInfo{};
		// Render pass layout the framebuffer will be used with
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		// List of attachments (1:1 with render pass, thanks to variable i)
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = swapchainExtent.width;
		framebufferCreateInfo.height = swapchainExtent.height;
		// Framebuffer layers
		framebufferCreateInfo.layers = 1;

		swapchainFramebuffers[i] = mainDevice.logicalDevice.createFramebuffer(framebufferCreateInfo);
	}
}

void VulkanRenderer::createGraphicsCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	vk::CommandPoolCreateInfo poolInfo{};
	// Queue family type that buffers from this command pool will use
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	graphicsCommandPool = mainDevice.logicalDevice.createCommandPool(poolInfo);
	;
}

void VulkanRenderer::createGraphicsCommandBuffers()
{
	// Create one command buffer for each framebuffer
	commandBuffers.resize(swapchainFramebuffers.size());

	vk::CommandBufferAllocateInfo commandBufferAllocInfo{}; // We are using a pool
	commandBufferAllocInfo.commandPool = graphicsCommandPool;
	commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	// Primary means the command buffer will submit directly to a queue.
	// Secondary cannot be called by a queue, but by an other primary command
	// buffer, via vkCmdExecuteCommands.
	commandBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;

	commandBuffers = mainDevice.logicalDevice.allocateCommandBuffers(commandBufferAllocInfo);
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	// How to begin each command buffer
	vk::CommandBufferBeginInfo commandBufferBeginInfo{};
	// Buffer can be resubmited when it has already been submited
	// commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

	// Information about how to being a render pass (only for graphical apps)
	vk::RenderPassBeginInfo renderPassBeginInfo{};
	// Render pass to begin
	renderPassBeginInfo.renderPass = renderPass;
	// Start point of render pass in pixel
	renderPassBeginInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	// Size of region to run render pass on
	renderPassBeginInfo.renderArea.extent = swapchainExtent;

	array<vk::ClearValue, 2> clearValues{};
	std::array<float, 4> colors{ 0.6f, 0.65f, 0.4f, 1.0f };
	clearValues[0].color = vk::ClearColorValue{ colors };
	clearValues[1].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	// Because 1-to-1 relationship
	renderPassBeginInfo.framebuffer = swapchainFramebuffers[currentImage];

	// Start recording commands to command buffer
	commandBuffers[currentImage].begin(commandBufferBeginInfo);

	// Begin render pass
	// All draw commands inline (no secondary command buffers)
	commandBuffers[currentImage].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

	// Bind pipeline to be used in render pass, you could switch pipelines for different subpasses
	commandBuffers[currentImage].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

	// Draw all meshes
	for (size_t j = 0; j < meshModels.size(); ++j)
	{
		// Push constants to given shader stage
		VulkanMeshModel model = meshModels[j];
		glm::mat4 modelMatrix = model.getModel();
		commandBuffers[currentImage].pushConstants(pipelineLayout,
			vk::ShaderStageFlagBits::eVertex, 0, sizeof(Model), &modelMatrix);

		// We have one model matrix for each object, then several children meshes

		for (size_t k = 0; k < model.getMeshCount(); ++k)
		{
			// Bind vertex buffer
			vk::Buffer vertexBuffers[] = { model.getMesh(k)->getVertexBuffer() };
			vk::DeviceSize offsets[] = { 0 };
			commandBuffers[currentImage].bindVertexBuffers(0, 1, vertexBuffers, offsets);

			// Bind index buffer
			commandBuffers[currentImage].bindIndexBuffer(
				model.getMesh(k)->getIndexBuffer(), 0, vk::IndexType::eUint32);

			// Bind descriptor sets
			array<vk::DescriptorSet, 2> descriptorSetsGroup{
					descriptorSets[currentImage],
					samplerDescriptorSets[model.getMesh(k)->getTexId()] };
			commandBuffers[currentImage].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout,
				0, static_cast<uint32_t>(descriptorSetsGroup.size()), descriptorSetsGroup.data(), 0, nullptr);

			// Execute pipeline
			commandBuffers[currentImage].drawIndexed(
				static_cast<uint32_t>(model.getMesh(k)->getIndexCount()), 1, 0, 0, 0);
		}
	}

	// End render pass
	commandBuffers[currentImage].endRenderPass();

	// Stop recordind to command buffer
	commandBuffers[currentImage].end();
}

void VulkanRenderer::createSynchronisation()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creation info
	vk::SemaphoreCreateInfo semaphoreCreateInfo{}; // That's all !

	// Fence creation info
	vk::FenceCreateInfo fenceCreateInfo{};
	// Fence starts open
	fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		imageAvailable[i] = mainDevice.logicalDevice.createSemaphore(semaphoreCreateInfo);
		renderFinished[i] = mainDevice.logicalDevice.createSemaphore(semaphoreCreateInfo);
		drawFences[i] = mainDevice.logicalDevice.createFence(fenceCreateInfo);
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	// -- UNIFORM VALUES DESCRIPTOR SETS LAYOUT --
	// ViewProjection binding information
	vk::DescriptorSetLayoutBinding vpLayoutBinding;
	// Binding number in shader
	vpLayoutBinding.binding = 0;
	// Type of descriptor (uniform, dynamic uniform, samples...)
	vpLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	// Number of descriptors for binding
	vpLayoutBinding.descriptorCount = 1;
	// Shader stage to bind to (here: vertex shader)
	vpLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	// For textures : can make sample data un changeable
	vpLayoutBinding.pImmutableSamplers = nullptr;

	vector<vk::DescriptorSetLayoutBinding> layoutBindings{
			vpLayoutBinding };

	// Descriptor set layout with given binding
	vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutCreateInfo.pBindings = layoutBindings.data();

	// Create descriptor set layout
	descriptorSetLayout = mainDevice.logicalDevice.createDescriptorSetLayout(layoutCreateInfo);

	// -- SAMPLER DESCRIPTOR SETS LAYOUT --
	vk::DescriptorSetLayoutBinding samplerLayoutBinding;
	// Binding 0 for descriptor set 1
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	vector<vk::DescriptorSetLayoutBinding> samplerLayoutBindings{ samplerLayoutBinding };

	vk::DescriptorSetLayoutCreateInfo textureLayoutCreateInfo{};
	textureLayoutCreateInfo.bindingCount =
		static_cast<uint32_t>(samplerLayoutBindings.size());
	textureLayoutCreateInfo.pBindings = samplerLayoutBindings.data();

	samplerDescriptorSetLayout = mainDevice.logicalDevice.createDescriptorSetLayout(textureLayoutCreateInfo);
}

void VulkanRenderer::createUniformBuffers()
{
	// Buffer size will be size of all 3 variables
	vk::DeviceSize vpBufferSize = sizeof(ViewProjection);

	// One uniform buffer for each image / each command buffer
	vpUniformBuffer.resize(swapchainImages.size());

	// Create uniform buffers
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		vpUniformBuffer[i].create(mainDevice.logicalDevice, mainDevice.physicalDevice, vpBufferSize,
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	}
}

void VulkanRenderer::createDescriptorPool()
{
	// One descriptor in the pool for each image

	// View projection pool
	vk::DescriptorPoolSize vpPoolSize{};
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	vector<vk::DescriptorPoolSize> poolSizes{ vpPoolSize };

	// One descriptor set that contains one descriptor
	vk::DescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCreateInfo.pPoolSizes = poolSizes.data();

	// Create pool
	descriptorPool = mainDevice.logicalDevice.createDescriptorPool(poolCreateInfo);

	// -- SAMPLER DESCRIPTOR POOL --
	// Texture sampler pool
	vk::DescriptorPoolSize samplerPoolSize{};
	// We assume one texture byobject.
	samplerPoolSize.descriptorCount = MAX_OBJECTS;

	vk::DescriptorPoolCreateInfo samplerPoolCreateInfo{};
	// The maximum for this is actually very high
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	samplerDescriptorPool = mainDevice.logicalDevice.createDescriptorPool(samplerPoolCreateInfo);
}

void VulkanRenderer::createDescriptorSets()
{
	// One descriptor set for every image/buffer
	descriptorSets.resize(swapchainImages.size());

	// We want the same layout for the right number of descriptor sets
	vector<vk::DescriptorSetLayout> setLayouts(swapchainImages.size(), descriptorSetLayout);

	// Allocation from the pool
	vk::DescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	// Allocate multiple descriptor sets
	vk::Result result = mainDevice.logicalDevice.allocateDescriptorSets(&setAllocInfo, descriptorSets.data());
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to allocate descriptor sets.");
	}

	// We have a connection between descriptor set layouts and descriptor sets,
	// but we don't know how link descriptor sets and the uniform buffers.

	// Update all of descriptor set buffer bindings
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		// -- VIEW PROJECTION DESCRIPTOR --
		// Description of the buffer and data offset
		vk::DescriptorBufferInfo vpBufferInfo{};
		vpBufferInfo.buffer = vpUniformBuffer[i].buffer;		// Buffer to get data from
		vpBufferInfo.offset = 0;								// We bind the whole data
		vpBufferInfo.range = sizeof(ViewProjection); // Size of data

		// Data about connection between binding and buffer
		vk::WriteDescriptorSet vpSetWrite{};
		vpSetWrite.dstSet = descriptorSets[i]; // Descriptor sets to update
		vpSetWrite.dstBinding = 0;						 // Binding to update (matches with shader binding)
		vpSetWrite.dstArrayElement = 0;				 // Index in array to update
		vpSetWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
		vpSetWrite.descriptorCount = 1;					// Amount of descriptor sets to update
		vpSetWrite.pBufferInfo = &vpBufferInfo; // Information about buffer data to bind

		vector<vk::WriteDescriptorSet> setWrites{ vpSetWrite };

		// Update descriptor set with new buffer/binding info
		mainDevice.logicalDevice.updateDescriptorSets(static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	// Copy view projection data
	void* data;
	vpUniformBuffer[imageIndex].map(sizeof(ViewProjection), 0);
	vpUniformBuffer[imageIndex].copyTo(&viewProjection, sizeof(ViewProjection));
	vpUniformBuffer[imageIndex].unmap();
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 modelP)
{
	if (modelId >= meshModels.size()) 
		return;

	meshModels[modelId].setModel(modelP);
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	// modelUniformAlignement = sizeof(Model) & ~(minUniformBufferOffet - 1);

	// We take the size of Model and we compare its size to a mask.
	// ~(minUniformBufferOffet - 1) is the inverse of minUniformBufferOffet
	// Example with a 16bits alignment coded on 8 bits:
	//   00010000 - 1  == 00001111
	// ~(00010000 - 1) == 11110000 which is our mask.
	// If we imagine our UboModel is 64 bits (01000000)
	// and the minUniformBufferOffet 16 bits (00010000),
	// (01000000) & ~(00010000 - 1) == 01000000 & 11110000 == 01000000
	// Our alignment will need to be 64 bits.

	// However this calculation is not perfect.

	// Let's now imagine our UboModel is 66 bits : 01000010.
	// The above calculation would give us a 64 bits alignment,
	// whereas we would need a 80 bits (01010000 = 64 + 16) alignment.

	// We need to add to the size minUniformBufferOffet - 1 to shield against this effect
	modelUniformAlignement = (sizeof(Model) + minUniformBufferOffet - 1) & ~(minUniformBufferOffet - 1);

	// We will now allocate memory for models.
	modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignement * MAX_OBJECTS, modelUniformAlignement);
}

void VulkanRenderer::createPushConstantRange()
{
	// Shader stage push constant will go to
	pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Model);
}

vk::Image VulkanRenderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, 
	vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling,
	vk::ImageUsageFlags useFlags, vk::MemoryPropertyFlags propFlags, vk::DeviceMemory* imageMemory)
{
	vk::ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.imageType = vk::ImageType::e2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	// Depth is 1, no 3D aspect
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = mipLevels;
	// Number of levels in image array
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	// How image data should be "tiled" (arranged for optimal reading)
	imageCreateInfo.tiling = tiling;
	// Initial layout in the render pass
	imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
	// Bit flags defining what image will be used for
	imageCreateInfo.usage = useFlags;
	// Number of samples for multi sampling
	imageCreateInfo.samples = numSamples;
	// Whether image can be shared between queues (no)
	imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;

	// Create the header of the image
	vk::Image image = mainDevice.logicalDevice.createImage(imageCreateInfo);

	// Now we need to setup and allocate memory for the image
	vk::MemoryRequirements memoryRequierements = mainDevice.logicalDevice.getImageMemoryRequirements(image);

	vk::MemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.allocationSize = memoryRequierements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice,
		memoryRequierements.memoryTypeBits, propFlags);

	auto result = mainDevice.logicalDevice.allocateMemory(&memoryAllocInfo, nullptr, imageMemory);
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to allocate memory for an image.");
	}

	// Connect memory to image
	mainDevice.logicalDevice.bindImageMemory(image, *imageMemory, 0);

	return image;
}

vk::Format VulkanRenderer::chooseSupportedFormat(const vector<vk::Format>& formats, vk::ImageTiling tiling, vk::FormatFeatureFlags featureFlags)
{
	// Loop through the options and find a compatible format
	for (vk::Format format : formats)
	{
		// Get properties for a given format on this device
		vk::FormatProperties properties = mainDevice.physicalDevice.getFormatProperties(format);

		// If the tiling is linear and all feature flags match
		if (tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		// If the tiling is optimal and all feature flags match
		else if (tiling == vk::ImageTiling::eOptimal &&
			(properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}
	throw std::runtime_error("Failed to find a matching format.");
}

void VulkanRenderer::createDepthBufferImage()
{
	std::vector<vk::Format> formats{
		// Look for a format with 32bits death buffer and stencil buffer
		vk::Format::eD32SfloatS8Uint,
		// if not found, without stencil
		vk::Format::eD32Sfloat,
		// if not 24bits depth and stencil buffer
		vk::Format::eD24UnormS8Uint };

	vk::Format depthFormat = chooseSupportedFormat(formats, vk::ImageTiling::eOptimal,
		// Format supports depth and stencil attachment
		vk::FormatFeatureFlagBits::eDepthStencilAttachment);

	// Create image and image view
	depthBufferImage = createImage(swapchainExtent.width, swapchainExtent.height, 1, 
		msaaSamples, depthFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, &depthBufferImageMemory);

	depthBufferImageView = createImageView(depthBufferImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

stbi_uc* VulkanRenderer::loadTextureFile(const string& filename, int* width, int* height, vk::DeviceSize* imageSize)
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

	*imageSize = *width * *height * 4; // RGBA has 4 channels
	return image;
}

int VulkanRenderer::createTextureImage(const string& filename, uint32_t& mipLevels)
{
	// Load image file
	int width, height;
	vk::DeviceSize imageSize;
	stbi_uc* imageData = loadTextureFile(filename, &width, &height, &imageSize);
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

	// Create staging buffer to hold loaded data, ready to copy to device
	Buffer imageStagingBuffer{ mainDevice.logicalDevice, mainDevice.physicalDevice,
		imageSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
	};

	// Copy image data to the staging buffer
	imageStagingBuffer.map(imageSize, 0);
	imageStagingBuffer.copyTo(imageData, static_cast<size_t>(imageSize));
	imageStagingBuffer.unmap();

	// Free original image data
	stbi_image_free(imageData);

	// Create image to hold final texture
	vk::Image texImage;
	vk::DeviceMemory texImageMemory;
	texImage = createImage(width, height, mipLevels, vk::SampleCountFlagBits::e1, 
		vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eDeviceLocal, &texImageMemory);

	// -- COPY DATA TO IMAGE --
	// Transition image to be DST for copy operations
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);

	// Copy image data
	imageStagingBuffer.copyToImage(texImage, width, height, graphicsQueue, graphicsCommandPool);

	// -- GENERATE MIPMAPS AND READY FOR SHADER USE --
	generateMipmaps(mainDevice.logicalDevice, mainDevice.physicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, vk::Format::eR8G8B8A8Srgb, width, height, mipLevels);

	// Add texture data to vector for reference
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy stagin buffers
	imageStagingBuffer.destroy();

	// Return index of new texture image
	return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(const string& filename)
{
	uint32_t mipLevels{ 0 };
	int textureImageLocation = createTextureImage(filename, mipLevels);

	vk::ImageView imageView = createImageView(textureImages[textureImageLocation],
		vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor, mipLevels);
	textureImageViews.push_back(imageView);

	int descriptorLoc = createTextureDescriptor(imageView);

	// Return location of set with texture
	return descriptorLoc;
}

void VulkanRenderer::createTextureSampler()
{
	vk::SamplerCreateInfo samplerCreateInfo{};
	// How to render when image is magnified on screen
	samplerCreateInfo.magFilter = vk::Filter::eLinear;
	// How to render when image is minified on screen
	samplerCreateInfo.minFilter = vk::Filter::eLinear;
	// Texture wrap in the U direction
	samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
	// Texture wrap in the V direction
	samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
	// Texture wrap in the W direction
	samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
	// When no repeat, texture become black beyond border
	samplerCreateInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
	// Coordinates ARE normalized. When true, coords are between 0 and image size
	samplerCreateInfo.unnormalizedCoordinates = false;
	// Fade between two mipmaps is linear
	samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	// Add a bias to the mimmap level
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 10.0f;
	// Overcome blur when a texture is stretched because of perspective with angle
	samplerCreateInfo.anisotropyEnable = true;
	// Anisotropy number of samples
	samplerCreateInfo.maxAnisotropy = 16;

	textureSampler = mainDevice.logicalDevice.createSampler(samplerCreateInfo);
}

int VulkanRenderer::createTextureDescriptor(vk::ImageView textureImageView)
{
	vk::DescriptorSet descriptorSet;

	vk::DescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerDescriptorSetLayout;

	vk::Result result = mainDevice.logicalDevice.allocateDescriptorSets(&setAllocInfo, &descriptorSet);
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to allocate texture descriptor set.");
	}

	// Texture image info
	vk::DescriptorImageInfo imageInfo{};
	// Image layout when in use
	imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	// Image view to bind to set
	imageInfo.imageView = textureImageView;
	// Sampler to use for set
	imageInfo.sampler = textureSampler;

	// Write info
	vk::WriteDescriptorSet descriptorWrite{};
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// Update new descriptor set
	mainDevice.logicalDevice.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	return samplerDescriptorSets.size() - 1;
}

int VulkanRenderer::createMeshModel(string filename)
{
	// Import model scene
	Assimp::Importer importer;
	// We want the model to be in triangles, to flip vertically texels uvs, and optimize the use of vertices
	const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate |
		aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
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
	vector<VulkanMesh> modelMeshes =
		VulkanMeshModel::loadNode(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, scene->mRootNode, scene, matToTex);

	auto meshModel = VulkanMeshModel(modelMeshes);
	meshModels.push_back(meshModel);
	return meshModels.size() - 1;
}

void VulkanRenderer::createColorBufferImage()
{
	vk::Format colorFormat = swapchainImageFormat;

	colorImage = createImage(swapchainExtent.width, swapchainExtent.height, 1, msaaSamples, colorFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, &colorImageMemory);
	colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}