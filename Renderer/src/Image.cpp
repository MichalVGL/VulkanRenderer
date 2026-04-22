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
	m_AspectFlags = aspectFlags;
	m_MipLevels = mipLevels;

	m_ImageView = dae::CreateImageView(device, image, format, aspectFlags, mipLevels);

	if(m_ImageView == VK_NULL_HANDLE)
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
	m_ImageRef.m_DeletionQueue.Flush(); //in case this image is being reinitialized

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
		m_ImageRef.m_DeletionQueue.PushDeleter([this, allocator = device.GetAllocator(), logicalDevice = device.GetLogicalDevice()]()
			{
				vmaDestroyImage(allocator, m_ImageRef.m_Image, m_Allocation);
				m_Allocation = VK_NULL_HANDLE;
				m_ImageRef.m_Image = VK_NULL_HANDLE;
			});
	}

	m_ImageRef.Init(device, image, format, aspectFlags, mipLevels);
}
void dae::Image::TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, std::optional<VkPipelineStageFlags2> destinationFlags, std::optional<VkAccessFlags2> destinationAccessMask)
{
	m_ImageRef.TransitionLayout(cmd, newLayout, destinationFlags, destinationAccessMask);
}

std::function<void()> dae::Image::GetDeletionFlush()
{
	return m_ImageRef.GetDeletionFlush();
}

