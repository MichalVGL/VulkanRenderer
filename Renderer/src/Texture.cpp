#include "Texture.h"

#include "Device.h"
#include "VulkanSetup.h"
#include "Buffer.h"

void dae::Texture::Init(const Device& device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels)
{
	m_DeletionQueue.Flush();

	// Initialize the image and view
	m_Image.Init(device, width, height, format, usage, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
	m_DeletionQueue.PushDeleter(m_Image.GetDeletionFlush());

	// Create the Sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device.GetPhysicalDevice(), &properties);

	samplerInfo.anisotropyEnable = GetRequiredDeviceFeatures().vulkanFeatures.features.samplerAnisotropy;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	if (vkCreateSampler(device.GetLogicalDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroySampler(logicalDevice, m_Sampler, nullptr);
				m_Sampler = VK_NULL_HANDLE;
			});
	}
}

void dae::Texture::InitFromFile(const Device& device, VkCommandPool commandPool, const std::filesystem::path& filepath)
{
	// read image
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture file: " + filepath.string());
    }

	// calculate mip levels
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    // init texture
    Init(device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, mipLevels);

    // load to gpu
    LoadPixels(device, commandPool, pixels, texWidth, texHeight);

	// unload image from cpu memory
    stbi_image_free(pixels);
}

void dae::Texture::LoadPixels(const Device& device, VkCommandPool commandPool, void* pixels, uint32_t width, uint32_t height)
{
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * 4);

    // buffer
    Buffer stagingBuffer;
    stagingBuffer.Init(device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	// copy data to buffer
    void* data = stagingBuffer.Map(device);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    stagingBuffer.Unmap(device);

	// create command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.GetLogicalDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

	// transition image
    m_Image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

	// copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), m_Image.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	// generate mipmaps
    m_Image.GenerateMipmaps(commandBuffer, width, height);

	// transition to shader read optimal
    m_Image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

	// submit command buffer
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

	VkQueue graphicsQueue = device.GetGraphicsQueue();
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

	// wait for completion
    vkQueueWaitIdle(graphicsQueue);

    // cleanup
    vkFreeCommandBuffers(device.GetLogicalDevice(), commandPool, 1, &commandBuffer);
}

std::function<void()> dae::Texture::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}
