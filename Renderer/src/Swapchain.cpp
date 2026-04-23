#include "Swapchain.h"

#include "Device.h"

void dae::Swapchain::Init(const Device& device, const Window& window, uint32_t& framesInFlight)
{
	//verify input
	if (window.IsInitialized() == false)
	{
		std::cerr << "[SwapChain.cpp] window given in initialization is not initialized!\n";
		throw std::invalid_argument("Window is not initialized!");
	}

	if (device.IsInitialized() == false)
	{
		std::cerr << "[SwapChain.cpp] device given in initialization is not initialized!\n";
		throw std::invalid_argument("Device is not initialized!");
	}

	GLFWwindow* pWindow = window.GetGLFWwindow();

	//in case of reinitialization, clear previous resources
	m_DeletionQueue.Flush();

	//get swapchain support details and imagecount
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device.GetPhysicalDevice(), device.GetSurface());

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, pWindow);
	
	//check if frames in flight is within the supported image count of the swapchain, if not adjust it
		// if maxImageCount is 0, there is no limit on the number of images in the swapchain
	if(swapChainSupport.capabilities.maxImageCount > 0 && framesInFlight > swapChainSupport.capabilities.maxImageCount)
	{
		framesInFlight = swapChainSupport.capabilities.maxImageCount;
		std::cout << "[SwapChain.cpp] frames in flight given in initialization is higher than the maximum image count supported by the swapchain! Adjusting frames in flight to " << framesInFlight << " to fit the swapchain support.\n";
	}
	else if (framesInFlight < swapChainSupport.capabilities.minImageCount)
	{
		framesInFlight = swapChainSupport.capabilities.minImageCount;
		std::cout << "[SwapChain.cpp] frames in flight given in initialization is lower than the minimum image count supported by the swapchain! Adjusting frames in flight to " << framesInFlight << " to fit the swapchain support.\n";
	}

	//create swapchain
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = device.GetSurface();
	createInfo.minImageCount = framesInFlight;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; //todo, test without TRANSFER_DST_BIT for clear. (validation layers)

	const QueueFamilyIndices& indices = device.GetQueueFamilyIndices();
	uint32_t queueFamilyIndices[2] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device.GetLogicalDevice(), &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroySwapchainKHR(logicalDevice, m_SwapChain, nullptr);
				m_SwapChain = VK_NULL_HANDLE;
			});
	}

	//get swapchain images and create ImageRefs for them
	vkGetSwapchainImagesKHR(device.GetLogicalDevice(), m_SwapChain, &framesInFlight, nullptr);
	std::vector<VkImage> images{};
	images.resize(framesInFlight);
	vkGetSwapchainImagesKHR(device.GetLogicalDevice(), m_SwapChain, &framesInFlight, images.data());
	m_SwapChainImages.resize(framesInFlight);
	for (size_t i = 0; i < images.size(); i++) {
		m_SwapChainImages[i] = std::make_unique<ImageRef>();
		m_SwapChainImages[i]->Init(device, images[i], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		m_DeletionQueue.PushDeleter(m_SwapChainImages[i]->GetDeletionFlush());
	}

	m_SwapChainImageFormat = surfaceFormat.format;
	m_SwapChainExtent = extent;

	m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
		{
			m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
			m_SwapChainExtent = { 0,0 };
			vkDeviceWaitIdle(logicalDevice);
		});
}

bool dae::Swapchain::IsInitialized() const
{
	return m_SwapChain != VK_NULL_HANDLE
		&& !m_SwapChainImages.empty()
		&& m_SwapChainImageFormat != VK_FORMAT_UNDEFINED
		&& m_SwapChainExtent.width != 0
		&& m_SwapChainExtent.height != 0;
}

std::function<void()> dae::Swapchain::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

dae::SwapChainSupportDetails dae::Swapchain::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails details{};
	
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

bool dae::Swapchain::CheckSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device, surface);
	return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty() 
		&& swapChainSupport.capabilities.maxImageCount > 0; //
}

VkSurfaceFormatKHR dae::Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	//could be handled by having fallback formats, but the project will not be adapted for these fallback options -> throw instead

	std::cerr << "[Swapchain.cpp] failed to find a suitable surface format!\n";
	throw std::runtime_error("failed to find a suitable surface format!");
}

VkPresentModeKHR dae::Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D dae::Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* pWindow)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(pWindow, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}
