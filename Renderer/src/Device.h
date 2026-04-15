#ifndef DEVICE_H
#define DEVICE_H

#include "pch.h"

#include "DeletionQueue.h"
#include "VulkanUtils.h"

namespace dae
{
	class Device final
	{
	public:

		void Init(GLFWwindow* pWindow);

		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetLogicalDevice() const { return m_Device; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }

		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }

	private:

		void CreateInstance();
		void CreateSurface(GLFWwindow* pWindow);
		void PickPhysicalDevice();
		void CreateLogicalDevice();

		bool IsDeviceSuitable(VkPhysicalDevice device);

		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

		VkInstance m_Instance;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device;

		VkSurfaceKHR m_Surface;

		QueueFamilyIndices m_QueueFamilyIndices;
		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		DeletionQueue m_DeletionQueue;
	};
}

#endif // !DEVICE_H
