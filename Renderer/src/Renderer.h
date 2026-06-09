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
		const Pipeline& GetGBufferPipeline() const { return m_GBufferPipeline; }
		const Pipeline& GetLightingPipeline() const { return m_LightingPipeline; }
		VkCommandPool GetCommandPool() const { return m_CommandPool; }

		std::function<void()> GetDeletionFlush();

	private:

		void DrawScene(VkCommandBuffer cmd, const Scene& scene);
		void CreateCommandPool();
		void InitializeGBuffer();
		void DeleteResources();

		VkRenderingAttachmentInfo MakeColorAttachment(VkImageView view);
		VkRenderingAttachmentInfo MakeDepthAttachment(VkImageView view, bool clearImageOnLoad, bool storeImage);
		void SetViewportScissor(VkCommandBuffer cmd);

		bool m_IsInitialized{ false };

		const Window* m_pWindow{};
		Device m_Device{};
		Swapchain m_Swapchain{};
		
		Pipeline m_DepthPrePassPipeline{};
		Pipeline m_GBufferPipeline{};
		Pipeline m_LightingPipeline{};

		VkCommandPool m_CommandPool{};

		std::vector<std::unique_ptr<Frame>> m_Frames{};
		int m_CurrentFrame{ 0 };
		uint32_t m_MaxFramesInFlight{ 3 };

		// GBuffer
		dae::Image m_AlbedoImage{};
		dae::Image m_NormalImage{};
		dae::Image m_MaterialImage{};
		VkSampler m_GBufferSampler{ VK_NULL_HANDLE };
		VkDescriptorPool m_GBufferDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet m_GBufferDescriptorSet{ VK_NULL_HANDLE };

		// Depth image
		dae::Image m_DepthImage{};

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !RENDERER_H
