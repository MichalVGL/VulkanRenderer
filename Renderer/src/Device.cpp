#include "Device.h"

#include <set>

//todo, delete this temp code, use a better extension system
std::vector<const char*> getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	//if (enableValidationLayers) {
	//	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	//}

	return extensions;
}

void dae::Device::Init(const Window& window)
{
	//verify input
	if (window.IsInitialized() == false)
	{
		std::cerr << "[Device.cpp] window given in initialization is not initialized!\n";
		throw std::invalid_argument("Window is not initialized!");
	}
	GLFWwindow* pWindow = window.GetGLFWwindow();

	//in case of reinitialization, clear previous resources
	m_DeletionQueue.Flush();

	CreateInstance();
	CreateSurface(pWindow);
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateAllocator();
}

bool dae::Device::IsInitialized() const
{
	return m_Device != VK_NULL_HANDLE
		&& m_PhysicalDevice != VK_NULL_HANDLE
		&& m_Surface != VK_NULL_HANDLE
		&& m_GraphicsQueue != VK_NULL_HANDLE
		&& m_PresentQueue != VK_NULL_HANDLE;
}

std::function<void()> dae::Device::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

void dae::Device::CreateInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	//todo, add validation layers setup logic
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;

	if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
		throw std::runtime_error("failed to create instance!");
	else
	{
		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroyInstance(m_Instance, nullptr);
				m_Instance = VK_NULL_HANDLE;
			});
	}
}

void dae::Device::CreateSurface(GLFWwindow* window)
{
	if (glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
				m_Surface = VK_NULL_HANDLE;
			});
	}
}

void dae::Device::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (IsDeviceSuitable(device)) {
			m_PhysicalDevice = device;
			break;
		}
	}

	if (m_PhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

void dae::Device::CreateLogicalDevice()
{
	m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice, m_Surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { m_QueueFamilyIndices.graphicsFamily.value(), m_QueueFamilyIndices.presentFamily.value() };

	float queuePriority{ 1.0f };
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	//
	//if (enableValidationLayers) {
	//	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	//	createInfo.ppEnabledLayerNames = validationLayers.data();
	//}
	//else {
	createInfo.enabledLayerCount = 0;
	//}

	if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroyDevice(m_Device, nullptr);
				m_Device = VK_NULL_HANDLE;
				m_PhysicalDevice = VK_NULL_HANDLE;
				m_GraphicsQueue = VK_NULL_HANDLE;
				m_PresentQueue = VK_NULL_HANDLE;
			});
	}

	vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.graphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.presentFamily.value(), 0, &m_PresentQueue);
}

void dae::Device::CreateAllocator()
{
	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_PhysicalDevice;
	allocatorInfo.device = m_Device;
	allocatorInfo.instance = m_Instance;

	if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS)
		throw std::runtime_error("failed to create VMA allocator!");

	m_DeletionQueue.PushDeleter([this]()
		{
			vmaDestroyAllocator(m_Allocator);
			m_Allocator = VK_NULL_HANDLE;
		});
}

//todo, delete this temp code, use a better extension system
bool dae::Device::IsDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = FindQueueFamilies(device, m_Surface);
	//
	//bool extensionsSupported = checkDeviceExtensionSupport(device);
	//
	//bool swapChainAdequate = false;
	//if (extensionsSupported) {
	//	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
	//	swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	//}
	//
	////week04 image extra
	//VkPhysicalDeviceFeatures supportedFeatures;
	//vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
	//
	//return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;

	return indices.IsComplete();
}

dae::QueueFamilyIndices dae::Device::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount{};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.IsComplete()) {
			break;
		}

		i++;
	}

	return indices;
}
