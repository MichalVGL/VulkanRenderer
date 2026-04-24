#ifndef IMAGE_H
#define IMAGE_H

#include "pch.h"

#include <optional>

#include "DeletionQueue.h"

namespace dae
{
	class Device;

	class ImageRef final
	{
	public:

		ImageRef() = default;
		~ImageRef() = default;

		ImageRef(const ImageRef&) = delete;
		ImageRef(ImageRef&&) = delete;
		ImageRef& operator=(const ImageRef&) = delete;
		ImageRef& operator=(ImageRef&&) = delete;

		bool IsInitialized() const;

		void Init(const Device& device, VkImage image
			, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels
			, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

		void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout
			, std::optional<VkPipelineStageFlags2> destinationFlags = std::nullopt, std::optional<VkAccessFlags2> destinationAccessMask = std::nullopt);

		VkImage GetImage() const { return m_Image; }
		VkImageView GetImageView() const { return m_ImageView; }
		VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
		uint32_t GetMipLevels() const { return m_MipLevels; }
		VkImageAspectFlags GetAspectFlags() const { return m_AspectFlags; }
		VkAccessFlags2 GetAccessMask() const { return m_CurrentAccessMask; }
		VkPipelineStageFlags2 GetPipelineStage() const { return m_CurrentStage; }

		std::function<void()> GetDeletionFlush();

	private:

		VkImage m_Image{ VK_NULL_HANDLE };
		VkImageView m_ImageView{ VK_NULL_HANDLE };
		VkImageLayout m_CurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		uint32_t m_MipLevels{ 1 };

		VkImageAspectFlags m_AspectFlags{ 0 };
		VkAccessFlags2 m_CurrentAccessMask{ 0 };
		VkPipelineStageFlags2 m_CurrentStage{ VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT };

		DeletionQueue m_DeletionQueue{};

		friend class Image;
	};

	class Image final
	{
	public:

		Image() = default;
		~Image() = default;

		Image(const Image&) = delete;
		Image(Image&&) = delete;
		Image& operator=(const Image&) = delete;
		Image& operator=(Image&&) = delete;

		bool IsInitialized() const;

		void Init(const Device& device, uint32_t width, uint32_t height
			, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);

		void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout
			, std::optional<VkPipelineStageFlags2> destinationFlags = std::nullopt, std::optional<VkAccessFlags2> destinationAccessMask = std::nullopt);

		void GenerateMipmaps(VkCommandBuffer cmd, int32_t texWidth, int32_t texHeight);

		VkImage GetImage() const { return m_ImageRef.GetImage(); }
		VkImageView GetImageView() const { return m_ImageRef.GetImageView(); }
		VkImageLayout GetCurrentLayout() const { return m_ImageRef.GetCurrentLayout(); }
		uint32_t GetMipLevels() const { return m_ImageRef.GetMipLevels(); }
		VkImageAspectFlags GetAspectFlags() const { return m_ImageRef.GetAspectFlags(); }
		VkAccessFlags2 GetAccessMask() const { return m_ImageRef.GetAccessMask(); }
		VkPipelineStageFlags2 GetPipelineStage() const { return m_ImageRef.GetPipelineStage(); }

		std::function<void()> GetDeletionFlush();

	private:

		VmaAllocation m_Allocation{ VK_NULL_HANDLE };

		ImageRef m_ImageRef{};

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !IMAGE_H
