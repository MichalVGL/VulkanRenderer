#include "Swapchain.h"

#include "Device.h"

void dae::Swapchain::Init(const Device& device, const Window& window)
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
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, pWindow);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	//create swapchain
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = device.GetSurface();
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //todo, add TRANSFER_DST_BIT for clear.

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

	//get swapchain images
	vkGetSwapchainImagesKHR(device.GetLogicalDevice(), m_SwapChain, &imageCount, nullptr);
	m_SwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device.GetLogicalDevice(), m_SwapChain, &imageCount, m_SwapChainImages.data());

	m_SwapChainImageFormat = surfaceFormat.format;
	m_SwapChainExtent = extent;

	//create image views
	m_SwapChainImageViews.resize(m_SwapChainImages.size());
	for (size_t i = 0; i < m_SwapChainImages.size(); i++) {
		m_SwapChainImageViews[i] = dae::CreateImageView(device, m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

		m_DeletionQueue.PushDeleter([this, i, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroyImageView(logicalDevice, m_SwapChainImageViews[i], nullptr);
				m_SwapChainImageViews[i] = VK_NULL_HANDLE;
			});
	}
}

bool dae::Swapchain::IsInitialized() const
{
	return m_SwapChain != VK_NULL_HANDLE
		&& !m_SwapChainImages.empty()
		&& m_SwapChainImageFormat != VK_FORMAT_UNDEFINED
		&& m_SwapChainExtent.width != 0
		&& m_SwapChainExtent.height != 0
		&& !m_SwapChainImageViews.empty();
}

std::function<void()> dae::Swapchain::GetDeletionFlush() //todo, use this for other classes aswell and use them in renderer
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

dae::SwapChainSupportDetails dae::Swapchain::QuerySwapChainSupport(const Device& device)
{
	SwapChainSupportDetails details{};

	VkPhysicalDevice physicalDevice = device.GetPhysicalDevice();
	VkSurfaceKHR surface = device.GetSurface();

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
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
