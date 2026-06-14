#include "CubeMap.h"

#include "Device.h"
#include "VulkanSetup.h"
#include "Image.h"
#include "Buffer.h"
#include "Pipeline.h"
#include "Texture.h"

void dae::CubeMap::Init(const Device& device, uint32_t resolution, VkFormat format, VkImageUsageFlags usage
	, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	m_DeletionQueue.Flush();

	m_Resolution = resolution;
	m_Format = format;

	m_AspectFlags = aspectFlags;

	uint32_t maxMipLevels = static_cast<uint32_t>(std::floor(std::log2(resolution))) + 1;
	m_MipLevels = std::min(mipLevels, maxMipLevels);
	m_MipLevels = std::max(m_MipLevels, 1u); // Ensure at least 1 mip level

	//=================================
	// Initialize the cube map

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = resolution;
	imageInfo.extent.height = resolution;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = m_MipLevels;
	imageInfo.arrayLayers = 6;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = m_CurrentLayout; //start with undefined
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

	CreateImageView(device);
	CreateSampler(device);
}

void dae::CubeMap::RenderFromImage(const Device& device, VkCommandPool commandPool, VkImageView sourceView, VkImageLayout sourceLayout, const std::filesystem::path& vertexShaderPath, const std::filesystem::path& fragmentShaderPath)
{
	DeletionQueue functionDeletionQueue{};

	std::array<VkImageView, 6> faceViews{};

	//create the image views for each face
	for (uint32_t face = 0; face < 6; face++)
	{
		VkImageViewCreateInfo faceViewInfo{};
		faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		faceViewInfo.image = m_Image;
		faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		faceViewInfo.format = m_Format;
		faceViewInfo.subresourceRange.aspectMask = m_AspectFlags;
		faceViewInfo.subresourceRange.baseMipLevel = 0;
		faceViewInfo.subresourceRange.levelCount = 1;
		faceViewInfo.subresourceRange.baseArrayLayer = face; // specific face
		faceViewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.GetLogicalDevice(), &faceViewInfo, nullptr, &faceViews[face]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view for cube map face!");
		}
		else
		{
			functionDeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice(), faceView = faceViews[face]]()
				{
					vkDestroyImageView(logicalDevice, faceView, nullptr);
				});
		}
	}

	//=======================================
	// projections and views

	glm::vec3 eye = glm::vec3(0.0f, 0.0f, 0.0f); // cube center

	glm::mat4 captureViews[] =
	{
		// +X
		glm::lookAt(eye, eye + glm::vec3(1.0f,  0.0f,  0.0f),
						 glm::vec3(0.0f, -1.0f,  0.0f)),

						 // -X
						 glm::lookAt(eye, eye + glm::vec3(-1.0f,  0.0f,  0.0f),
										  glm::vec3(0.0f, -1.0f,  0.0f)),

										  // -Y
										  glm::lookAt(eye, eye + glm::vec3(0.0f, -1.0f,  0.0f),
														   glm::vec3(0.0f,  0.0f, -1.0f)),

														   // +Y
														   glm::lookAt(eye, eye + glm::vec3(0.0f,  1.0f,  0.0f),
																			glm::vec3(0.0f,  0.0f,  1.0f)),

																			// +Z
																			glm::lookAt(eye, eye + glm::vec3(0.0f,  0.0f,  1.0f),
																							 glm::vec3(0.0f, -1.0f,  0.0f)),

																							 // -Z
																							 glm::lookAt(eye, eye + glm::vec3(0.0f,  0.0f, -1.0f),
																											  glm::vec3(0.0f, -1.0f,  0.0f)),
	};

	glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	// GLM fix for OpenGL-style Y coordinate in projection matrices
	captureProj[1][1] *= -1;

	//==============================
	// Required resources for conversion shader

	//create sampler
	VkSampler sourceSampler{ VK_NULL_HANDLE };
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;

	if (vkCreateSampler(device.GetLogicalDevice(), &samplerInfo, nullptr, &sourceSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler for hdri image!");
	}
	else
	{
		functionDeletionQueue.PushDeleter([logicalDevice = device.GetLogicalDevice(), sourceSampler]()
			{
				vkDestroySampler(logicalDevice, sourceSampler, nullptr);
			});
	}

	// descriptor pool
	VkDescriptorPool sourcePool{ VK_NULL_HANDLE };
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(device.GetLogicalDevice(), &poolInfo, nullptr, &sourcePool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool for hdri image!");
	}
	else
	{
		functionDeletionQueue.PushDeleter([logicalDevice = device.GetLogicalDevice(), sourcePool]()
			{
				vkDestroyDescriptorPool(logicalDevice, sourcePool, nullptr);
			});
	}

	//==============================
	// Pipeline

	VkPushConstantRange pushConstants{}; //constains view and projection matrices
	pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstants.offset = 0;
	pushConstants.size = sizeof(ConversionPushConstants);

	// create pipeline and descriptor sets for the conversion shader
	PipelineInfo pipelineInfo{};
	pipelineInfo.vertexShaderFilePath = vertexShaderPath.c_str();
	pipelineInfo.fragmentShaderFilePath = fragmentShaderPath.c_str();
	pipelineInfo.colorAttachmentFormats = { m_Format };
	pipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineInfo.cullMode = VK_CULL_MODE_NONE;
	pipelineInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineInfo.depthTestEnable = VK_FALSE;
	pipelineInfo.depthWriteEnable = VK_FALSE;
	pipelineInfo.pushConstantRange = pushConstants;
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
	allocDescInfo.descriptorPool = sourcePool;
	allocDescInfo.descriptorSetCount = 1;
	allocDescInfo.pSetLayouts = &pipeline.GetDescriptorSetLayout();

	if (vkAllocateDescriptorSets(device.GetLogicalDevice(), &allocDescInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor set!");
	}

	// write HDRI image into descriptor set
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = sourceLayout;
	imageInfo.imageView = sourceView;
	imageInfo.sampler = sourceSampler;

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
	if (vkAllocateCommandBuffers(device.GetLogicalDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS)
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

	uint32_t size = m_Resolution;

	for (uint32_t face = 0; face < 6; face++)
	{
		// target this face as color attachment
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = faceViews[face]; // per-face view
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea = { {0, 0}, {size, size} };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		vkCmdBeginRendering(commandBuffer, &renderingInfo);

		// set viewport and scissor
		VkViewport viewport{};
		viewport.width = static_cast<float>(size);
		viewport.height = static_cast<float>(size);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { size, size };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// push face index
		ConversionPushConstants pushConstants{};
		pushConstants.captureProj = captureProj;
		pushConstants.captureView = captureViews[face];
		vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ConversionPushConstants), &pushConstants);

		vkCmdDraw(commandBuffer, 36, 1, 0, 0); // unit cube
		vkCmdEndRendering(commandBuffer);

	}

	// Prepare for mipmap generation
	TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);

	GenerateMipmaps(commandBuffer);

	// Final transition (must outside the mipmaps in case miplevel = 1)
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

// miplevel = -1 means all levels, otherwise only the specified level
void dae::CubeMap::TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, std::optional<VkPipelineStageFlags2> destinationFlags, std::optional<VkAccessFlags2> destinationAccessMask, int mipLevel)
{
	uint32_t baseMip = (mipLevel == -1) ? 0 : mipLevel;
	uint32_t mipCount = (mipLevel == -1) ? m_MipLevels : 1;

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
	barrier.subresourceRange.baseMipLevel = baseMip;
	barrier.subresourceRange.levelCount = mipCount;
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

void dae::CubeMap::CreateImageView(const Device& device)
{
	VkImageViewCreateInfo cubeViewInfo{};
	cubeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	cubeViewInfo.image = m_Image;
	cubeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	cubeViewInfo.format = m_Format;

	cubeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cubeViewInfo.subresourceRange.baseMipLevel = 0;
	cubeViewInfo.subresourceRange.levelCount = m_MipLevels;
	cubeViewInfo.subresourceRange.baseArrayLayer = 0;
	cubeViewInfo.subresourceRange.layerCount = 6;

	if (vkCreateImageView(device.GetLogicalDevice(), &cubeViewInfo, nullptr, &m_MainImageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create main image view for cube map!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroyImageView(logicalDevice, m_MainImageView, nullptr);
				m_MainImageView = VK_NULL_HANDLE;
			});
	}
}

void dae::CubeMap::GenerateMipmaps(VkCommandBuffer cmd)
{
	if (m_MipLevels <= 1) return;

	int32_t mipRes = m_Resolution;

	for (uint32_t mip = 1; mip < m_MipLevels; ++mip)
	{
		uint32_t dstSize = std::max(1u, m_Resolution >> mip);

		for (uint32_t face = 0; face < 6; ++face)
		{
			// Transition previous mip to SRC
			if (mip > 1)
			{
				VkImageMemoryBarrier2 barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.image = m_Image;
				barrier.subresourceRange.aspectMask = m_AspectFlags;
				barrier.subresourceRange.baseMipLevel = mip - 1;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = face;
				barrier.subresourceRange.layerCount = 1;

				VkDependencyInfo depInfo{};
				depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
				depInfo.imageMemoryBarrierCount = 1;
				depInfo.pImageMemoryBarriers = &barrier;

				vkCmdPipelineBarrier2(cmd, &depInfo);
			}

			// Transition destination mip to DST
			{
				VkImageMemoryBarrier2 barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
				barrier.srcAccessMask = 0;
				barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.image = m_Image;
				barrier.subresourceRange.aspectMask = m_AspectFlags;
				barrier.subresourceRange.baseMipLevel = mip;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = face;
				barrier.subresourceRange.layerCount = 1;

				VkDependencyInfo depInfo{};
				depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
				depInfo.imageMemoryBarrierCount = 1;
				depInfo.pImageMemoryBarriers = &barrier;

				vkCmdPipelineBarrier2(cmd, &depInfo);
			}

			// Blit one face
			VkImageBlit2 blit{};
			blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipRes, mipRes, 1 };
			blit.srcSubresource.aspectMask = m_AspectFlags;
			blit.srcSubresource.mipLevel = mip - 1;
			blit.srcSubresource.baseArrayLayer = face;
			blit.srcSubresource.layerCount = 1;

			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] =
			{
				static_cast<int32_t>(dstSize),
				static_cast<int32_t>(dstSize),
				1
			};
			blit.dstSubresource.aspectMask = m_AspectFlags;
			blit.dstSubresource.mipLevel = mip;
			blit.dstSubresource.baseArrayLayer = face;
			blit.dstSubresource.layerCount = 1;

			VkBlitImageInfo2 blitInfo{};
			blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
			blitInfo.srcImage = m_Image;
			blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			blitInfo.dstImage = m_Image;
			blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			blitInfo.regionCount = 1;
			blitInfo.pRegions = &blit;
			blitInfo.filter = VK_FILTER_LINEAR;

			vkCmdBlitImage2(cmd, &blitInfo);
		}

		mipRes = dstSize;
	}

	{
		VkImageMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.image = m_Image;
		barrier.subresourceRange.aspectMask = m_AspectFlags;
		barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 6;

		VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
		vkCmdPipelineBarrier2(cmd, &depInfo);
	}
}

