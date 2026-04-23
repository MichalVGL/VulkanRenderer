#include "Device.h"

#include <set>

#include "VulkanSetup.h"
#include "Swapchain.h"

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
	SetupDebugMessenger();
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

bool dae::Device::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> available(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

	for (const auto& required : g_DeviceExtensions)
	{
		bool found = false;
		for (const auto& ext : available)
		{
			if (strcmp(required, ext.extensionName) == 0)
			{
				found = true;
				break;
			}
		}
		if (!found) return false;
	}
	return true;
}

bool dae::Device::CheckDeviceFeatureSupport(VkPhysicalDevice device)
{
	VulkanDeviceFeatures available{};
	vkGetPhysicalDeviceFeatures2(device, &available.vulkanFeatures);

	const VulkanDeviceFeatures& requested = dae::GetRequiredDeviceFeatures();

	return dae::AreRequiredFeaturesAvailable(available, requested);
}

void dae::Device::CreateInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = dae::GetRequiredInstanceExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (g_EnableValidationLayers)
	{
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
	
		createInfo.pNext = &debugCreateInfo;
		createInfo.enabledLayerCount = static_cast<uint32_t>(dae::g_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = dae::g_ValidationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

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

void dae::Device::SetupDebugMessenger()
{
	if (!g_EnableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this]()
			{
				DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
				m_DebugMessenger = VK_NULL_HANDLE;
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

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = nullptr;
	createInfo.pNext = &dae::GetRequiredDeviceFeatures().vulkanFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(dae::g_DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = dae::g_DeviceExtensions.data();

	if (g_EnableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(dae::g_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = dae::g_ValidationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

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

bool dae::Device::IsDeviceSuitable(VkPhysicalDevice device)
{
	if (m_Surface == VK_NULL_HANDLE)
	{
		std::cerr << "[Device.cpp] Surface is not initialized when checking device suitability!\n";
		throw std::runtime_error("Surface is not initialized!");
	}

	QueueFamilyIndices indices = FindQueueFamilies(device, m_Surface);

	bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		swapChainAdequate = Swapchain::CheckSwapChainSupport(device, m_Surface);
	}

	return indices.IsComplete() && extensionsSupported && swapChainAdequate && CheckDeviceFeatureSupport(device);
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
