#ifndef IMAGE_H
#define IMAGE_H

#include "pch.h"

#include "DeletionQueue.h"

namespace dae
{
	class Device;

	class ImageRef final
	{
	public:

		//should be Init(VKImage image, ...)
		void Init(const Device& device, VkImage image
			, VkFormat format, VkImageAspectFlags aspectFlags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

		void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout);

		VkImage GetImage() const;
		VkImageView GetImageView() const;
		VkImageLayout GetCurrentLayout() const;

	private:

		VkImage m_Image{ VK_NULL_HANDLE };
		VkImageView m_ImageView{ VK_NULL_HANDLE };

		int m_MipLevels{ 1 };

		//todo, start here
		VkAccessFlags m_CurrentAccessMask{ 0 };
		VkPipelineStageFlags m_CurrentStage{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		VkImageLayout m_CurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };

		DeletionQueue m_DeletionQueue{};

		friend class Image;
	};

	class Image final
	{
	public:

		void Init(const Device& device, uint32_t width, uint32_t height
			, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags);

	private:

		VmaAllocation m_Allocation{ VK_NULL_HANDLE };

		ImageRef m_ImageRef{};
	};
}

#endif // !IMAGE_H
