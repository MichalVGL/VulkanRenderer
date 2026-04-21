#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "pch.h"

#include "VulkanUtils.h"
#include "DeletionQueue.h"
#include "Window.h"

namespace dae
{
	class Device;

	class Swapchain final
	{
	public:

		void Init(const Device& device, const Window& window);

		VkSwapchainKHR GetSwapChain() const { return m_SwapChain; }

		bool IsInitialized() const;
		std::function<void()> GetDeletionFlush();

		static SwapChainSupportDetails QuerySwapChainSupport(const Device& device);

	private:

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* pWindow);

		VkSwapchainKHR m_SwapChain{};
		std::vector<VkImage> m_SwapChainImages{};
		VkFormat m_SwapChainImageFormat{};
		VkExtent2D m_SwapChainExtent{};
		std::vector<VkImageView> m_SwapChainImageViews{};

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !SWAPCHAIN_H
