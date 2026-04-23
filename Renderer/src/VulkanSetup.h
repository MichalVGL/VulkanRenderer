#ifndef VULKANSETUP_H
#define VULKANSETUP_H

#include "pch.h"
#include "VulkanUtils.h"

namespace dae
{
	//===============================
	// Swapchain and Frames in Flight
	//===============================
	
	constexpr uint32_t g_DesiredMaxFramesInFlight = 3;

	//===============================
	// Validation Layers
	//===============================

	constexpr bool g_EnableValidationLayers =
#ifdef DEBUG
		true;
#else
		false;
#endif // DEBUG

	inline const std::vector<const char*> g_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	//===============================
	// Instance Extensions
	//===============================

	inline std::vector<const char*> GetRequiredInstanceExtensions() {
		//glfw requirements
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		//validation layers requirements
		if (g_EnableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		//other requirements
		std::initializer_list<const char*> otherExtensions = {
			//add them here

		};

		extensions.reserve(extensions.size() + otherExtensions.size());
		std::ranges::copy(otherExtensions, std::back_inserter(extensions));

		return extensions;
	}

	//===============================
	// Device Extensions
	//===============================

	inline const std::vector<const char*> g_DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	//===============================
	// Features
	//===============================

	inline VulkanDeviceFeatures& GetRequiredDeviceFeatures() {
		static bool initialized = false;
		static VulkanDeviceFeatures requiredDeviceFeatures;

		if (!initialized)
		{
			// Set the features here
			requiredDeviceFeatures.vulkanFeatures.features.samplerAnisotropy = VK_TRUE;
			requiredDeviceFeatures.vulkan13Features.dynamicRendering = VK_TRUE;
			requiredDeviceFeatures.vulkan13Features.synchronization2 = VK_TRUE;

			requiredDeviceFeatures.vulkan12Features.runtimeDescriptorArray = VK_TRUE;
			requiredDeviceFeatures.vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
			requiredDeviceFeatures.vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
			requiredDeviceFeatures.vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
			requiredDeviceFeatures.vulkan12Features.descriptorIndexing = VK_TRUE;
			requiredDeviceFeatures.vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

			initialized = true;
		}

		return requiredDeviceFeatures;
	}

}

#endif // !VULKANSETUP_H
