#ifndef CUBEMAP_H
#define CUBEMAP_H

#include "pch.h"

#include "DeletionQueue.h"

namespace dae
{
	class Device;
	class Image;

	struct ConversionPushConstants final
	{
		glm::mat4 captureProj;
		glm::mat4 captureView;
	};

	class CubeMap final
	{
	public:
		CubeMap() = default;
		~CubeMap() = default;

		CubeMap(const CubeMap&) = delete;
		CubeMap(CubeMap&&) = delete;
		CubeMap& operator=(const CubeMap&) = delete;
		CubeMap& operator=(CubeMap&&) = delete;

		void Init(const Device& device, uint32_t resolution, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 999);
		
		void RenderFromImage(const Device& device, VkCommandPool commandPool, VkImageView sourceView, VkImageLayout sourceLayout
			, const std::filesystem::path& vertexShaderPath, const std::filesystem::path& fragmentShaderPath);

		void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout
			, std::optional<VkPipelineStageFlags2> destinationFlags, std::optional<VkAccessFlags2> destinationAccessMask, int mipLevel = -1); //-1 = all levels

		VkImage GetImage() const { return m_Image; }
		VkImageView GetImageView() const { return m_MainImageView; }
		VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
		VkSampler GetSampler() const { return m_Sampler; }

		std::function<void()> GetDeletionFlush();

	private:

		void CreateSampler(const Device& device);
		void CreateImageView(const Device& device);
		void GenerateMipmaps(VkCommandBuffer cmd);

		VkImage m_Image{};
		VkImageView m_MainImageView{ VK_NULL_HANDLE };
		VkFormat m_Format{};
		VkImageLayout m_CurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		uint32_t m_Resolution{};
		uint32_t m_MipLevels{ 1 };

		VmaAllocation m_Allocation{ VK_NULL_HANDLE };

		VkImageAspectFlags m_AspectFlags{ 0 };
		VkAccessFlags2 m_CurrentAccessMask{ 0 };
		VkPipelineStageFlags2 m_CurrentStage{ VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT };

		VkSampler m_Sampler{ VK_NULL_HANDLE };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !CUBEMAP_H
