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
#include "CubeMap.h"
#include "Texture.h"

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

		void DrawScene(VkCommandBuffer cmd, const Scene& scene, Frame& currentFrame, const Pipeline& pipeline);

		void CreateLayoutBindings();
		void InitializeImages();
		void InitializePipelines();
		void CreateCommandPool();
		void InitializeGBufferDescriptors();
		void InitializeHDRImage();
		void InitializeIBL();
		void DeleteResources();

		VkRenderingAttachmentInfo MakeColorAttachment(VkImageView view);
		VkRenderingAttachmentInfo MakeDepthAttachment(VkImageView view, bool clearImageOnLoad, bool storeImage, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		void SetViewportScissor(VkCommandBuffer cmd);

		bool m_IsInitialized{ false };

		const Window* m_pWindow{};
		Device m_Device{};
		Swapchain m_Swapchain{};
		
		Pipeline m_DepthPrePassPipeline{};
		Pipeline m_GBufferPipeline{};
		Pipeline m_LightingPipeline{};
		Pipeline m_BlittingPipeline{};

		VkCommandPool m_CommandPool{};

		std::vector<std::unique_ptr<Frame>> m_Frames{};
		int m_CurrentFrame{ 0 };
		uint32_t m_MaxFramesInFlight{ 3 };

		//Scene data, only layout here (data is in the frame itself)
		VkDescriptorSetLayout m_SceneDataLayout{ VK_NULL_HANDLE };

		//Lights data, only layout here (data is in the frame itself)
		VkDescriptorSetLayout m_LightLayout{ VK_NULL_HANDLE };

		// GBuffer
		dae::Image m_AlbedoImage{};
		dae::Image m_NormalImage{};
		dae::Image m_MaterialImage{};
		VkSampler m_GBufferSampler{ VK_NULL_HANDLE };
		VkDescriptorPool m_GBufferDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet m_GBufferDescriptorSet{ VK_NULL_HANDLE };

		//HDR Image
		Image m_HDRImage{};
		VkSampler m_HDRSampler{ VK_NULL_HANDLE };
		VkDescriptorPool m_HDRDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet m_HDRDescriptorSet{ VK_NULL_HANDLE };

		//sky box 
		CubeMap m_SkyBox{};
		VkDescriptorPool m_SkyBoxDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_SkyboxLayout{ VK_NULL_HANDLE };
		VkDescriptorSet m_SkyBoxDescriptorSet{ VK_NULL_HANDLE };

		//IBL diffuse box 
		CubeMap m_IBLDiffuse{};
		VkDescriptorPool m_IBLDiffuseDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_IBLDiffuseLayout{ VK_NULL_HANDLE };
		VkDescriptorSet m_IBLDiffuseDescriptorSet{ VK_NULL_HANDLE };

		// Depth image
		Image m_DepthImage{};

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !RENDERER_H
