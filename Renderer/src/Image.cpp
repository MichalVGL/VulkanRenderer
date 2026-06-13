#include "Image.h"

#include "Device.h"

// ===================================================
// ImageRef
// ===================================================

bool dae::ImageRef::IsInitialized() const
{
	return m_Image != VK_NULL_HANDLE
		&& m_ImageView != VK_NULL_HANDLE
		&& m_MipLevels > 0;
}

void dae::ImageRef::Init(const Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageLayout initialLayout)
{
	m_DeletionQueue.Flush(); //in case this image is being reinitialized

	m_Image = image;
	m_CurrentLayout = initialLayout;
	m_Format = format;
	m_AspectFlags = aspectFlags;
	m_MipLevels = mipLevels;

	m_ImageView = dae::CreateImageView(device, image, format, aspectFlags, mipLevels);

	if (m_ImageView == VK_NULL_HANDLE)
		throw std::runtime_error("failed to create image view!");
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroyImageView(logicalDevice, m_ImageView, nullptr);
				m_ImageView = VK_NULL_HANDLE;
			});
	}
}

void dae::ImageRef::TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, std::optional<VkPipelineStageFlags2> destinationFlags, std::optional<VkAccessFlags2> destinationAccessMask)
{
	//if the new stage and access mask are not specified, we will use the current ones
	VkPipelineStageFlags2 dstStageMask = destinationFlags.has_value() ? destinationFlags.value() : m_CurrentStage;
	VkAccessFlags2 dstAccessMask = destinationAccessMask.has_value() ? destinationAccessMask.value() : m_CurrentAccessMask;

	//create the barrier
	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = m_CurrentStage;
	barrier.srcAccessMask = m_CurrentAccessMask;
	barrier.oldLayout = m_CurrentLayout;
	barrier.dstStageMask = dstStageMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = m_Image;
	barrier.subresourceRange.aspectMask = m_AspectFlags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = m_MipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	//submit the barrier
	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);

	//save the new layout
	m_CurrentLayout = newLayout;
	m_CurrentAccessMask = dstAccessMask;
	m_CurrentStage = dstStageMask;
}

std::function<void()> dae::ImageRef::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

// ===================================================
// Image
// ===================================================

bool dae::Image::IsInitialized() const
{
	return m_Allocation != VK_NULL_HANDLE
		&& m_ImageRef.IsInitialized();
}

void dae::Image::Init(const Device& device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	m_DeletionQueue.Flush();

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkImage image{};
	if (vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &image, &m_Allocation, nullptr) != VK_SUCCESS)
		throw std::runtime_error("failed to create image!");
	else
	{
		m_DeletionQueue.PushDeleter([this, allocator = device.GetAllocator(), logicalDevice = device.GetLogicalDevice()]()
			{
				vmaDestroyImage(allocator, m_ImageRef.m_Image, m_Allocation);
				m_Allocation = VK_NULL_HANDLE;
				m_ImageRef.m_Image = VK_NULL_HANDLE;
			});
	}

	m_ImageRef.Init(device, image, format, aspectFlags, mipLevels);
	m_DeletionQueue.PushDeleter(m_ImageRef.GetDeletionFlush());
}
void dae::Image::TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, std::optional<VkPipelineStageFlags2> destinationFlags, std::optional<VkAccessFlags2> destinationAccessMask)
{
	m_ImageRef.TransitionLayout(cmd, newLayout, destinationFlags, destinationAccessMask);
}

void dae::Image::GenerateMipmaps(VkCommandBuffer cmd, int32_t texWidth, int32_t texHeight)
{
	int32_t mipLevels = m_ImageRef.m_MipLevels;

	if (mipLevels <= 1) return; //nothing to generate

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		// Transition to TRANSFER_SRC for the previous mip level
		{
			VkImageMemoryBarrier2 barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.image = m_ImageRef.m_Image;
			barrier.subresourceRange.aspectMask = m_ImageRef.m_AspectFlags;
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			VkDependencyInfo depInfo{};
			depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			depInfo.imageMemoryBarrierCount = 1;
			depInfo.pImageMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2(cmd, &depInfo);
		}

		// 2. Transition mip i: UNDEFINED -> TRANSFER_DST
		{
			VkImageMemoryBarrier2 barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = 0;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.image = m_ImageRef.m_Image;
			barrier.subresourceRange = { m_ImageRef.m_AspectFlags, i, 1, 0, 1 };

			VkDependencyInfo dep{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
								  .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
			vkCmdPipelineBarrier2(cmd, &dep);
		}

		//blit
		VkImageBlit2 blit{};
		blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = m_ImageRef.m_AspectFlags;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = m_ImageRef.m_AspectFlags;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		VkBlitImageInfo2 blitInfo{};
		blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
		blitInfo.srcImage = m_ImageRef.m_Image;
		blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		blitInfo.dstImage = m_ImageRef.m_Image;
		blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		blitInfo.regionCount = 1;
		blitInfo.pRegions = &blit;
		blitInfo.filter = VK_FILTER_LINEAR;

		vkCmdBlitImage2(cmd, &blitInfo);

		// -> SHADER_READ_ONLY
		{
			VkImageMemoryBarrier2 barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.image = m_ImageRef.m_Image;
			barrier.subresourceRange = { m_ImageRef.m_AspectFlags, i - 1, 1, 0, 1 };

			VkDependencyInfo dep{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
								  .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
			vkCmdPipelineBarrier2(cmd, &dep);
		}

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition the very last mip level from TRANSFER_DST to SHADER_READ_ONLY
	VkImageMemoryBarrier2 finalBarrier{};
	finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	finalBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	finalBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	finalBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	finalBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	finalBarrier.image = m_ImageRef.m_Image;
	finalBarrier.subresourceRange.aspectMask = m_ImageRef.m_AspectFlags;
	finalBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
	finalBarrier.subresourceRange.levelCount = 1;
	finalBarrier.subresourceRange.baseArrayLayer = 0;
	finalBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo finalDepInfo{};
	finalDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	finalDepInfo.imageMemoryBarrierCount = 1;
	finalDepInfo.pImageMemoryBarriers = &finalBarrier;

	vkCmdPipelineBarrier2(cmd, &finalDepInfo);

	// Update internal tracking
	m_ImageRef.m_CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_ImageRef.m_CurrentStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	m_ImageRef.m_CurrentAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
}

std::function<void()> dae::Image::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

