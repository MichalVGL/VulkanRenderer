#include "CubeMap.h"

#include "Device.h"
#include "VulkanSetup.h"
#include "Image.h"
#include "Buffer.h"
#include "Pipeline.h"

/*
void dae::CubeMap::Init(const Device& device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels)
{
	m_DeletionQueue.Flush();

	
	// Initialize the image and view
	//m_Image.Init(device, width, height, format, usage, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
	//m_DeletionQueue.PushDeleter(m_Image.GetDeletionFlush());

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
		throw std::runtime_error("[Texture] failed to create texture sampler!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroySampler(logicalDevice, m_Sampler, nullptr);
				m_Sampler = VK_NULL_HANDLE;
			});
	}
}*/

void dae::CubeMap::InitFromFile(const Device& device, VkCommandPool commandPool, const std::filesystem::path& filepath, uint32_t resolution, VkFormat format, VkImageAspectFlags aspectFlags)
{
	m_DeletionQueue.Flush();

	m_Resolution = resolution;
	m_Format = format;
	m_AspectFlags = aspectFlags;

	DeletionQueue functionDeletionQueue{};

	//=================================
	// Load the hdri image into a temporary image

	//load the temp image
	int texWidth, texHeight, texChannels;

	float* pixels = stbi_loadf(filepath.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	uint64_t imageSize = static_cast<uint64_t>(texWidth) * texHeight * 4 * sizeof(float);

	if (!pixels) {
		throw std::runtime_error("Failed to load texture file: " + filepath.string());
	}

	Image tempImage{};

	tempImage.Init(device, texWidth, texHeight, format,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	functionDeletionQueue.PushDeleter(tempImage.GetDeletionFlush());

	//todo, add shader to copy the 2d image to the cubemap, then remove tempImage

	Buffer stagingBuffer;
	stagingBuffer.Init(device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	functionDeletionQueue.PushDeleter(stagingBuffer.GetDeletionFlush());

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
	tempImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
	region.imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), tempImage.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	// transition to shader read optimal
	tempImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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

	//=================================
	// Initialize the cube map

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = resolution;
	imageInfo.extent.height = resolution;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 6;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = m_CurrentLayout;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //todo, determine the correct flags
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (vmaCreateImage(device.GetAllocator(), &imageInfo, &vmaAllocInfo, &m_Image, &m_Allocation, nullptr) != VK_SUCCESS)
		throw std::runtime_error("failed to create image!");
	else
	{
		m_DeletionQueue.PushDeleter([this, allocator = device.GetAllocator(), logicalDevice = device.GetLogicalDevice()]()
			{
				vmaDestroyImage(allocator, m_Image, m_Allocation);
				m_Allocation = VK_NULL_HANDLE;
				m_Image = VK_NULL_HANDLE;
			});
	}

	//create the image views for each face
	for (uint32_t i = 0; i < 6; i++)
	{
		VkImageViewCreateInfo faceViewInfo{};
		faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		faceViewInfo.image = m_Image;
		faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		faceViewInfo.format = format;
		faceViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		faceViewInfo.subresourceRange.baseMipLevel = 0;
		faceViewInfo.subresourceRange.levelCount = 1;
		faceViewInfo.subresourceRange.baseArrayLayer = i; // specific face
		faceViewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.GetLogicalDevice(), &faceViewInfo, nullptr, &m_FaceViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view for cube map face!");
		}
		else
		{
			m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice(), faceView = m_FaceViews[i]]()
				{
					vkDestroyImageView(logicalDevice, faceView, nullptr);
				});
		}
	}

	//=================================
	// Fill the cube map with the hdri data

	// Image now loaded in gpu, copy to cubemap using shader
	ConvertHDRIToCubemap(device, commandPool, tempImage);

	//set the main view
	VkImageViewCreateInfo cubeViewInfo{};
	cubeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	cubeViewInfo.image = m_Image;
	cubeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	cubeViewInfo.format = m_Format;

	cubeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cubeViewInfo.subresourceRange.baseMipLevel = 0;
	cubeViewInfo.subresourceRange.levelCount = 1;
	cubeViewInfo.subresourceRange.baseArrayLayer = 0;
	cubeViewInfo.subresourceRange.layerCount = 6;

	if(vkCreateImageView(device.GetLogicalDevice(), &cubeViewInfo, nullptr, &m_MainImageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create main image view for cube map!");
	} else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroyImageView(logicalDevice, m_MainImageView, nullptr);
				m_MainImageView = VK_NULL_HANDLE;
			});
	}

	//create the sampler
	CreateSampler(device);
}

void dae::CubeMap::TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, std::optional<VkPipelineStageFlags2> destinationFlags, std::optional<VkAccessFlags2> destinationAccessMask)
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
	barrier.subresourceRange.layerCount = 6;

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



std::function<void()> dae::CubeMap::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

void dae::CubeMap::RenderFromImage(const Device& device, VkCommandPool commandPool, const Image& image, const std::filesystem::path& vertexShaderPath, const std::filesystem::path& fragmentShaderPath)
{

}

void dae::CubeMap::ConvertHDRIToCubemap(const Device& device, VkCommandPool commandPool, const Image& hdriImage)
{
	DeletionQueue functionDeletionQueue{};

	//==============================
	// Required resources for conversion shader

	//create sampler
	VkSampler hdriSampler{ VK_NULL_HANDLE };
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;

	if (vkCreateSampler(device.GetLogicalDevice(), &samplerInfo, nullptr, &hdriSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler for hdri image!");
	}
	else
	{
		functionDeletionQueue.PushDeleter([logicalDevice = device.GetLogicalDevice(), hdriSampler]()
			{
				vkDestroySampler(logicalDevice, hdriSampler, nullptr);
			});
	}

	// descriptor set layout
	//VkDescriptorSetLayout hdriLayout{ VK_NULL_HANDLE };
	//VkDescriptorSetLayoutBinding hdriBinding{};
	//hdriBinding.binding = 0;
	//hdriBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//hdriBinding.descriptorCount = 1;
	//hdriBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	//
	//VkDescriptorSetLayoutCreateInfo layoutInfo{};
	//layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	//layoutInfo.bindingCount = 1;
	//layoutInfo.pBindings = &hdriBinding;

	//if (vkCreateDescriptorSetLayout(device.GetLogicalDevice(), &layoutInfo, nullptr, &hdriLayout) != VK_SUCCESS)
	//{
	//	throw std::runtime_error("Failed to create descriptor set layout for hdri image!");
	//}
	//else
	//{
	//	functionDeletionQueue.PushDeleter([logicalDevice = device.GetLogicalDevice(), hdriLayout]()
	//		{
	//			vkDestroyDescriptorSetLayout(logicalDevice, hdriLayout, nullptr);
	//		});
	//}

	// descriptor pool
	VkDescriptorPool hdriPool{ VK_NULL_HANDLE };
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(device.GetLogicalDevice(), &poolInfo, nullptr, &hdriPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool for hdri image!");
	}
	else
	{
		functionDeletionQueue.PushDeleter([logicalDevice = device.GetLogicalDevice(), hdriPool]()
			{
				vkDestroyDescriptorPool(logicalDevice, hdriPool, nullptr);
			});
	}

	//==============================
	// Pipeline

	VkPushConstantRange faceIndex{};
	faceIndex.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	faceIndex.offset = 0;
	faceIndex.size = sizeof(uint32_t);

	// create pipeline and descriptor sets for the conversion shader
	PipelineInfo pipelineInfo{};
	pipelineInfo.vertexShaderFilePath = "resources/shaders/CubeMapConvert.vert.spv";
	pipelineInfo.fragmentShaderFilePath = "resources/shaders/CubeMapConvert.frag.spv";
	pipelineInfo.colorAttachmentFormats = { m_Format };
	pipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineInfo.cullMode = VK_CULL_MODE_NONE;
	pipelineInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineInfo.depthTestEnable = VK_FALSE;
	pipelineInfo.depthWriteEnable = VK_FALSE;
	pipelineInfo.pushConstantRange = faceIndex;
	pipelineInfo.texturesDescriptorLayout = TexturesDescriptorLayoutType::SingleImage;
	pipelineInfo.additionalSetLayouts = {};

	Pipeline pipeline{};
	pipeline.Init(device, pipelineInfo);
	functionDeletionQueue.PushDeleter(pipeline.GetDeletionFlush());

	//==============================
	// descriptor set

	// allocate descriptor set
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetAllocateInfo allocDescInfo{};
	allocDescInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocDescInfo.descriptorPool = hdriPool;
	allocDescInfo.descriptorSetCount = 1;
	allocDescInfo.pSetLayouts = &pipeline.GetDescriptorSetLayout();

	if(vkAllocateDescriptorSets(device.GetLogicalDevice(), &allocDescInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor set!");
	}

	// write HDRI image into descriptor set
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = hdriImage.GetImageView();
	imageInfo.sampler = hdriSampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device.GetLogicalDevice(), 1, &write, 0, nullptr);

	//==============================
	// Command buffer
	
	// create command buffer
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	if(vkAllocateCommandBuffers(device.GetLogicalDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffer!");
	}
	else
	{
		functionDeletionQueue.PushDeleter([&, commandPool, logicalDevice = device.GetLogicalDevice()]()
			{
				vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
				commandBuffer = VK_NULL_HANDLE;
			});
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	//==============================
	// execute gpu conversion

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	//transition cubemap to be written to
	TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	pipeline.Bind(commandBuffer);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS
		, pipeline.GetPipelineLayout()
		, 0, 1, &descriptorSet, 0, nullptr);

	for (uint32_t face = 0; face < 6; face++)
	{
		// target this face as color attachment
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = m_FaceViews[face]; // per-face view
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea = { {0, 0}, {m_Resolution, m_Resolution} };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		vkCmdBeginRendering(commandBuffer, &renderingInfo);

		// set viewport and scissor
		VkViewport viewport{};
		viewport.width = static_cast<float>(m_Resolution);
		viewport.height = static_cast<float>(m_Resolution);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { m_Resolution, m_Resolution };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		//pipeline.Bind(commandBuffer);

		// push face index
		uint32_t faceIndex = face;
		vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &faceIndex);

		vkCmdDraw(commandBuffer, 3, 1, 0, 0); // fullscreen triangle
		vkCmdEndRendering(commandBuffer);
	}

	TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkQueue graphicsQueue = device.GetGraphicsQueue();
	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

	// wait for completion
	vkQueueWaitIdle(graphicsQueue);
}

void dae::CubeMap::CreateSampler(const Device& device)
{
	// Create the Sampler
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

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
		throw std::runtime_error("[Texture] failed to create texture sampler!");
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

