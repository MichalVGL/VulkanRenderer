#ifndef VULKANUTILS_H
#define VULKANUTILS_H

#include "pch.h"

#include <optional>

namespace dae
{
	class Device;

	//===============================
	// Concepts
	//===============================

	template<typename T>
	concept VulkanFeature = std::same_as<T, VkPhysicalDeviceVulkan13Features>
		|| std::same_as<T, VkPhysicalDeviceVulkan12Features>
		|| std::same_as<T, VkPhysicalDeviceVulkan11Features>
		|| std::same_as<T, VkPhysicalDeviceFeatures2>;

	//===============================
	// Structs
	//===============================

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		bool IsComplete() const
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats{};
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct VulkanDeviceFeatures
	{
		VulkanDeviceFeatures()
		{
			vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
			vulkanFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

			vulkanFeatures.pNext = &vulkan11Features;
			vulkan11Features.pNext = &vulkan12Features;
			vulkan12Features.pNext = &vulkan13Features;
			vulkan13Features.pNext = nullptr;
		}

		VulkanDeviceFeatures(const VulkanDeviceFeatures&) = delete;
		VulkanDeviceFeatures(VulkanDeviceFeatures&&) = delete;
		VulkanDeviceFeatures& operator=(const VulkanDeviceFeatures&) = delete;
		VulkanDeviceFeatures& operator=(VulkanDeviceFeatures&&) = delete;

		VkPhysicalDeviceVulkan13Features vulkan13Features{};
		VkPhysicalDeviceVulkan12Features vulkan12Features{};
		VkPhysicalDeviceVulkan11Features vulkan11Features{};
		VkPhysicalDeviceFeatures2 vulkanFeatures{};
	};

	//===============================
	// Functions
	//===============================

	// Image View

	VkImageView CreateImageView(const Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	// Feature Checking

	template<VulkanFeature T>
	inline bool AreRequiredFeaturesAvailable(const T& available, const T& requested)
	{
		//this function should ideally use reflection (C++26)

		// offset past sType (uint32_t) and pNext (void*)
		constexpr size_t offset = sizeof(VkStructureType) + sizeof(void*);

		const VkBool32* requestedPtr = reinterpret_cast<const VkBool32*>(
			reinterpret_cast<const char*>(&requested) + offset);
		const VkBool32* availablePtr = reinterpret_cast<const VkBool32*>(
			reinterpret_cast<const char*>(&available) + offset);

		//amount of features in the struct (amount of VK_BOOL32)
		size_t count = (sizeof(T) - sizeof(VkStructureType) - sizeof(void*)) / sizeof(VkBool32);
		//				(total size		-		sType		-			pNext) / feature size 
		//loop and compare all VK_BOOL32
		for (size_t i = 0; i < count; i++)
		{
			if (requestedPtr[i] && !availablePtr[i])
				return false;
		}
		return true;
	}

	inline bool AreRequiredFeaturesAvailable(const VulkanDeviceFeatures& available, const VulkanDeviceFeatures& requested)
	{
		return AreRequiredFeaturesAvailable(available.vulkan13Features, requested.vulkan13Features)
			&& AreRequiredFeaturesAvailable(available.vulkan12Features, requested.vulkan12Features)
			&& AreRequiredFeaturesAvailable(available.vulkan11Features, requested.vulkan11Features)
			&& AreRequiredFeaturesAvailable(available.vulkanFeatures, requested.vulkanFeatures);
	}

	//Debug Messenger

	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	inline void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}
}

#endif // !VULKANUTILS_H
