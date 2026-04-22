#include "Image.h"

#include "Device.h"

// ===================================================
// ImageRef
// ===================================================

void dae::ImageRef::Init(const Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageLayout initialLayout)
{
	m_DeletionQueue.Flush(); //in case this image is being reinitialized

	m_Image = image;
	m_CurrentLayout = initialLayout;

	m_ImageView = dae::CreateImageView(device, image, format, aspectFlags, 1); //todo, add mipLevels as member var

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

void dae::ImageRef::TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout)
{

}

// ===================================================
// Image
// ===================================================

void dae::Image::Init(const Device& device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags)
{
	m_ImageRef.m_DeletionQueue.Flush(); //in case this image is being reinitialized

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
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

	m_ImageRef.Init(device, image, format, aspectFlags);
}	
