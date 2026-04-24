#ifndef RENDERER_H
#define RENDERER_H

#include "pch.h"

#include "DeletionQueue.h"
#include "VulkanUtils.h"
#include "Device.h"
#include "Swapchain.h"
#include "Window.h"
#include "Frame.h"
#include "Pipeline.h"

namespace dae
{

	class Frame;
	class Scene;

	class Renderer final
	{
	public:

		Renderer() = default;

		void Init(const Window* pWindow);
		
		void DrawFrame(const Scene& scene);

		const Device& GetDevice() const { return m_Device; }
		const Swapchain& GetSwapchain() const { return m_Swapchain; }
		const Pipeline& GetPipeline() const { return m_Pipeline; }
		VkCommandPool GetCommandPool() const { return m_CommandPool; }

		std::function<void()> GetDeletionFlush();

	private:

		void DrawScene(VkCommandBuffer cmd, const Scene& scene);
		void CreateCommandPool();
		void DeleteResources();

		bool m_IsInitialized{ false };

		const Window* m_pWindow{};
		Device m_Device{};
		Swapchain m_Swapchain{};
		Pipeline m_Pipeline{};

		VkCommandPool m_CommandPool{};

		std::vector<std::unique_ptr<Frame>> m_Frames{};
		int m_CurrentFrame{ 0 };
		uint32_t m_MaxFramesInFlight{ 3 };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !RENDERER_H
