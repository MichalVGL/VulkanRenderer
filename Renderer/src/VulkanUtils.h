#ifndef VULKANUTILS_H
#define VULKANUTILS_H

#include "pch.h"

#include <optional>

namespace dae
{
	class Device;

	//Type Aliases

	// signals that the resource is managed by that class (prevent confusion from raw pointers)
	template<typename T>
	using ManagedPtr = T*;

	//Helper Structs

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

	//Helper Functions

	VkImageView CreateImageView(const Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
}

#endif // !VULKANUTILS_H
