#ifndef FRAME_H
#define FRAME_H

#include "pch.h"
#include "VulkanUtils.h"

#include "DeletionQueue.h"
#include "GraphicsTypes.h"
#include "Buffer.h"

namespace dae
{
	class Device;
	class Swapchain;

	class Frame
	{
	public:

		Frame() = default;
		~Frame() = default;
		
		Frame(const Frame&) = delete;
		Frame(Frame&&) = delete;
		Frame& operator=(const Frame&) = delete;
		Frame& operator=(Frame&&) = delete;

		void Init(const Device& device, VkCommandPool commandPool, VkDescriptorSetLayout sceneDataLayout);

		// sync
		void WaitForFence(const Device& device);
		void ResetFence(const Device& device);

		// scene data
		void UpdateSceneData(const SceneData& data);
		VkDescriptorSet GetSceneDataDescriptorSet() const { return m_SceneDataDescriptorSet; }

		// command recording
		VkCommandBuffer BeginCommands();
		void EndCommands();

		// submission and presentation
		void Submit(const Device& device);
		void Present(const Device& device, const Swapchain& swapchain, uint32_t imageIndex);

		// getters
		VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }
		VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailableSemaphore; }
		VkSemaphore GetRenderFinishedSemaphore() const { return m_RenderFinishedSemaphore; }
		VkFence GetInFlightFence() const { return m_InFlightFence; }
		std::function<void()> GetDeletionFlush();

	private:

		void CreateCommandBuffer(const Device& device, VkCommandPool commandPool);
		void CreateSyncObjects(const Device& device);
		void CreateSceneDataBuffer(const Device& device, VkDescriptorSetLayout sceneDataLayout);

		VkCommandBuffer m_CommandBuffer{};
		VkSemaphore m_ImageAvailableSemaphore{};
		VkSemaphore m_RenderFinishedSemaphore{};
		VkFence m_InFlightFence{};

		//scene data
		Buffer m_SceneDataBuffer{};
		void* m_SceneDataMapped{ nullptr };
		VkDescriptorPool m_SceneDataDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet m_SceneDataDescriptorSet{ VK_NULL_HANDLE };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !FRAME_H
