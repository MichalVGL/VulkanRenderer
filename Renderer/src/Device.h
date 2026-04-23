#ifndef DEVICE_H
#define DEVICE_H

#include "pch.h"

#include "DeletionQueue.h"
#include "VulkanUtils.h"
#include "Window.h"

namespace dae
{
	class Device final
	{
	public:

		void Init(const Window& window);

		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetLogicalDevice() const { return m_Device; }
		VmaAllocator GetAllocator() const { return m_Allocator; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }

		bool IsInitialized() const;
		std::function<void()> GetDeletionFlush();

		static bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		static bool CheckDeviceFeatureSupport(VkPhysicalDevice device);

	private:

		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface(GLFWwindow* pWindow);
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateAllocator();

		bool IsDeviceSuitable(VkPhysicalDevice device);

		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

		VkInstance m_Instance{ VK_NULL_HANDLE };
		VkPhysicalDevice m_PhysicalDevice{ VK_NULL_HANDLE };
		VkDevice m_Device{ VK_NULL_HANDLE };
		VmaAllocator m_Allocator{ VK_NULL_HANDLE };
		VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };

		VkDebugUtilsMessengerEXT m_DebugMessenger{ VK_NULL_HANDLE };

		QueueFamilyIndices m_QueueFamilyIndices{};
		VkQueue m_GraphicsQueue{ VK_NULL_HANDLE };
		VkQueue m_PresentQueue{ VK_NULL_HANDLE };
		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !DEVICE_H
