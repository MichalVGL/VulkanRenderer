#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "pch.h"

#include "VulkanUtils.h"
#include "DeletionQueue.h"
#include "Window.h"
#include "Image.h"

namespace dae
{
	class Device;

	class Swapchain final
	{
	public:

		void Init(const Device& device, const Window& window, uint32_t& framesInFlight);

		VkSwapchainKHR GetSwapChain() const { return m_SwapChain; }

		bool IsInitialized() const;
		std::function<void()> GetDeletionFlush();
		ImageRef& GetImage(size_t index) const { return *m_SwapChainImages[index]; }
		int GetImageCount() const { return static_cast<int>(m_SwapChainImages.size()); }
		VkFormat GetImageFormat() const { return m_SwapChainImageFormat; }
		VkExtent2D GetExtent() const { return m_SwapChainExtent; }

		static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		static bool CheckSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	private:

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* pWindow);

		VkSwapchainKHR m_SwapChain{};
		//std::vector<VkImage> m_SwapChainImages{};
		std::vector<std::unique_ptr<ImageRef>> m_SwapChainImages{};
		VkFormat m_SwapChainImageFormat{};
		VkExtent2D m_SwapChainExtent{};

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !SWAPCHAIN_H
