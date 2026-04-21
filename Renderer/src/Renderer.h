#ifndef RENDERER_H
#define RENDERER_H

#include "pch.h"

#include "DeletionQueue.h"
#include "VulkanUtils.h"
#include "Device.h"
#include "Swapchain.h"
#include "Window.h"

namespace dae
{
	//class RendererBuilder final
	//{
	//public:
	//
	//
	//
	//private:
	//
	//
	//}

	class Frame;

	class Renderer final
	{
	public:

		Renderer() = default;

		void Init();

		void DrawFrame();

	private:

		void CreateCommandPool();

		void DeleteResources();

		bool m_IsInitialized{ false };

		Window m_Window{};
		Device m_Device{};
		Swapchain m_Swapchain{};

		VkCommandPool m_CommandPool{};

		std::vector<Frame> m_Frames{};
		int m_CurrentFrame{ 0 };
		int m_MaxFramesInFlight{ 2 };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !RENDERER_H
