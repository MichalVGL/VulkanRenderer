#include "Renderer.h"

#include "VulkanSetup.h"
#include "Scene.h"

void dae::Renderer::Init(const Window* pWindow)
{
	//verify input
	if (pWindow == nullptr)
	{
		std::cerr << "[Renderer.cpp] window given in initialization is a null pointer!\n";
		throw std::invalid_argument("Window pointer is null!");
	}
	else if (pWindow->IsInitialized() == false)
	{
		std::cerr << "[Renderer.cpp] window given in initialization is not initialized!\n";
		throw std::invalid_argument("Window is not initialized!");
	}

	m_pWindow = pWindow;

	if (m_IsInitialized)
	{
		DeleteResources(); //todo, remove this and make a reinit function
	}

	// Device
	m_Device.Init(*m_pWindow);
	m_DeletionQueue.PushDeleter(m_Device.GetDeletionFlush());

	m_MaxFramesInFlight = g_DesiredMaxFramesInFlight;

	// Swapchain
	m_Swapchain.Init(m_Device, *m_pWindow, m_MaxFramesInFlight); //might change max frames in flight
	m_DeletionQueue.PushDeleter(m_Swapchain.GetDeletionFlush());

	//==============================
	// Images for deferred rendering 

	VkExtent2D extent = m_Swapchain.GetExtent();
	// Albedo image
	m_AlbedoImage.Init(m_Device, extent.width, extent.height,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, 1);
	m_DeletionQueue.PushDeleter(m_AlbedoImage.GetDeletionFlush());

	// Normal image
	m_NormalImage.Init(m_Device, extent.width, extent.height,
		VK_FORMAT_R16G16_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, 1);
	m_DeletionQueue.PushDeleter(m_NormalImage.GetDeletionFlush());

	// Material image (roughness, metallic, ao)
	m_MaterialImage.Init(m_Device, extent.width, extent.height,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, 1);
	m_DeletionQueue.PushDeleter(m_MaterialImage.GetDeletionFlush());

	// Depth image
	m_DepthImage.Init(m_Device, extent.width, extent.height,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	m_DeletionQueue.PushDeleter(m_DepthImage.GetDeletionFlush());

	//==============================
	// Deferred Pipelines 

	// GBuffer Pipeline
	PipelineInfo gBufferPipelineInfo{};
	gBufferPipelineInfo.vertexShaderFilePath = "resources/shaders/GBuffer.vert.spv";
	gBufferPipelineInfo.fragmentShaderFilePath = "resources/shaders/GBuffer.frag.spv";
	gBufferPipelineInfo.colorAttachmentFormats = { m_AlbedoImage.GetFormat(), m_NormalImage.GetFormat(), m_MaterialImage.GetFormat() };
	gBufferPipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	gBufferPipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	gBufferPipelineInfo.depthAttachmentFormat = m_DepthImage.GetFormat();
	gBufferPipelineInfo.descriptorLayout = DescriptorLayoutType::BindlessTextures;

	m_GBufferPipeline.Init(m_Device, gBufferPipelineInfo);
	m_DeletionQueue.PushDeleter(m_GBufferPipeline.GetDeletionFlush());

	// Lighting Pipeline
	PipelineInfo lightingPipelineInfo{};
	lightingPipelineInfo.vertexShaderFilePath = "resources/shaders/Lighting.vert.spv";
	lightingPipelineInfo.fragmentShaderFilePath = "resources/shaders/Lighting.frag.spv";
	lightingPipelineInfo.colorAttachmentFormats = { m_Swapchain.GetImageFormat() };
	lightingPipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	lightingPipelineInfo.cullMode = VK_CULL_MODE_NONE;
	lightingPipelineInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	lightingPipelineInfo.descriptorLayout = DescriptorLayoutType::GBuffer;

	m_LightingPipeline.Init(m_Device, lightingPipelineInfo);
	m_DeletionQueue.PushDeleter(m_LightingPipeline.GetDeletionFlush());

	//Setup frames and command pool

	CreateCommandPool();
	m_Frames.resize(m_MaxFramesInFlight);

	for (int i = 0; i < m_MaxFramesInFlight; ++i)
		m_Frames[i] = std::make_unique<Frame>();

	for (auto& frame : m_Frames)
	{
		frame->Init(m_Device, m_CommandPool);
		m_DeletionQueue.PushDeleter(frame->GetDeletionFlush());
	}

	m_DeletionQueue.PushDeleter([this]()
		{
			vkDeviceWaitIdle(m_Device.GetLogicalDevice());
		});

	// GBuffer
	InitializeGBuffer();

	m_IsInitialized = true;
}

void dae::Renderer::DrawFrame(const Scene& scene)
{
	Frame& currentFrame = *m_Frames[m_CurrentFrame];

	currentFrame.WaitForFence(m_Device);

	uint32_t imageIndex{};
	VkResult result = vkAcquireNextImageKHR(m_Device.GetLogicalDevice(), m_Swapchain.GetSwapChain(), UINT64_MAX, currentFrame.GetImageAvailableSemaphore(), VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		//todo, add resizing handling here
		//recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	currentFrame.ResetFence(m_Device);

	VkCommandBuffer commandBuffer = currentFrame.BeginCommands();

	ImageRef& image = m_Swapchain.GetImage(imageIndex);

	//==============================
	// Geometry Pass

	//GBuffer
	m_AlbedoImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
	m_NormalImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
	m_MaterialImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	// Depth
	m_DepthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

	// setup GBuffer attachments
	std::array<VkRenderingAttachmentInfo, 3> gBufferAttachments{};
	gBufferAttachments[0] = MakeColorAttachment(m_AlbedoImage.GetImageView());
	gBufferAttachments[1] = MakeColorAttachment(m_NormalImage.GetImageView());
	gBufferAttachments[2] = MakeColorAttachment(m_MaterialImage.GetImageView());

	// depth attachment
	VkRenderingAttachmentInfo depthAttachment = MakeDepthAttachment(m_DepthImage.GetImageView());

	VkRenderingInfo geometryRenderingInfo{};
	geometryRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	geometryRenderingInfo.renderArea = { {0, 0}, m_Swapchain.GetExtent() };
	geometryRenderingInfo.layerCount = 1;
	geometryRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(gBufferAttachments.size());
	geometryRenderingInfo.pColorAttachments = gBufferAttachments.data();
	geometryRenderingInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(commandBuffer, &geometryRenderingInfo);

	// set dynamic viewport and scissor
	SetViewportScissor(commandBuffer);

	//draw scene to gbuffer (GBuffer pipeline is bound inside this function)
	DrawScene(commandBuffer, scene);
	vkCmdEndRendering(commandBuffer);

	//==============================
	// Lighting Pass

	// Transition GBuffer for reading
	m_AlbedoImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);
	m_NormalImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);
	m_MaterialImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);

	// SwapImage
	image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	VkRenderingAttachmentInfo swapAttachment = MakeColorAttachment(image.GetImageView());

	VkRenderingInfo lightingRenderingInfo{};
	lightingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	lightingRenderingInfo.renderArea = { {0, 0}, m_Swapchain.GetExtent() };
	lightingRenderingInfo.layerCount = 1;
	lightingRenderingInfo.colorAttachmentCount = 1;
	lightingRenderingInfo.pColorAttachments = &swapAttachment;
	// no depth attachment

	vkCmdBeginRendering(commandBuffer, &lightingRenderingInfo);
	SetViewportScissor(commandBuffer);
	m_LightingPipeline.Bind(commandBuffer);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_LightingPipeline.GetPipelineLayout(), 0, 1, &m_GBufferDescriptorSet, 0, nullptr);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0); // fullscreen triangle, hardcoded in shader
	vkCmdEndRendering(commandBuffer);

	//==============================
	// Transition to present

	image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		VK_ACCESS_2_NONE);

	currentFrame.EndCommands();
	currentFrame.Submit(m_Device);
	currentFrame.Present(m_Device, m_Swapchain, imageIndex);

	m_CurrentFrame = (m_CurrentFrame + 1) % m_MaxFramesInFlight;
}

std::function<void()> dae::Renderer::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

void dae::Renderer::DrawScene(VkCommandBuffer cmd, const Scene& scene)
{
	//get the viewprojection matrix
	glm::mat4 view = scene.GetCamera().GetCameraToWorld();

	m_GBufferPipeline.Bind(cmd);

	VkDescriptorSet globalSet = scene.GetDescriptorSet();
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS
		, m_GBufferPipeline.GetPipelineLayout(),
		0, 1, &globalSet, 0, nullptr);

	glm::mat4 proj = glm::perspective(
		glm::radians(scene.GetCamera().GetFOV()),
		static_cast<float>(m_Swapchain.GetExtent().width) / static_cast<float>(m_Swapchain.GetExtent().height),
		scene.GetCamera().m_Near,
		scene.GetCamera().m_Far
	);

	proj[1][1] *= -1;

	glm::mat4 viewProj = proj * view;

	for (const auto& renderObject : scene.GetRenderObjects())
	{
		if (!renderObject.pMesh
			|| !renderObject.pMaterial)
		{
			continue;
		}

		PushConstants push{};
		push.transform = viewProj * renderObject.transform;
		push.vertexBufferAddress = renderObject.pMesh->GetVertexBufferAddress(m_Device);
		push.albedoTextureIndex = renderObject.pMaterial->albedoTextureIndex;
		push.normalTextureIndex = renderObject.pMaterial->normalTextureIndex;
		push.metalicRoughnessTextureIndex = renderObject.pMaterial->metalicRoughnessTextureIndex;

		vkCmdPushConstants(cmd, m_GBufferPipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(PushConstants), &push);

		vkCmdBindIndexBuffer(cmd,
			renderObject.pMesh->GetIndexBuffer().GetBuffer(),
			0, VK_INDEX_TYPE_UINT32);

		for (const auto& primitive : renderObject.pMesh->GetPrimitives())
		{
			vkCmdDrawIndexed(cmd, primitive.indexCount, 1,
				primitive.firstIndex, 0, 0);
		}
	}
}

void dae::Renderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_Device.GetQueueFamilyIndices().graphicsFamily.value();

	if (vkCreateCommandPool(m_Device.GetLogicalDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroyCommandPool(m_Device.GetLogicalDevice(), m_CommandPool, nullptr);
				m_CommandPool = VK_NULL_HANDLE;
			});
	}
}

void dae::Renderer::InitializeGBuffer()
{
	//===================================
	//Shared GBuffer Sampler 

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;

	if (vkCreateSampler(m_Device.GetLogicalDevice(), &samplerInfo, nullptr, &m_GBufferSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("[Renderer] failed to create GBuffer sampler!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = m_Device.GetLogicalDevice()]()
			{
				vkDestroySampler(logicalDevice, m_GBufferSampler, nullptr);
				m_GBufferSampler = VK_NULL_HANDLE;
			});
	}

	//===================================
	// Descriptor Pool

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 3;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if(vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolInfo, nullptr, &m_GBufferDescriptorPool) != VK_SUCCESS) 
	{
		throw std::runtime_error("[Renderer] failed to create GBuffer descriptor pool!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = m_Device.GetLogicalDevice()]()
			{
				vkDestroyDescriptorPool(logicalDevice, m_GBufferDescriptorPool, nullptr);
				m_GBufferDescriptorPool = VK_NULL_HANDLE;
			});
	}
	//===================================
	// Descriptor sets

	// Allocate the descriptor set

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_GBufferDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_LightingPipeline.GetDescriptorSetLayout();

	if(vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocInfo, &m_GBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("[Renderer] failed to allocate GBuffer descriptor set!");
	}

	// fill in the descriptor sets

	std::vector<VkDescriptorImageInfo> imageInfos{};
	imageInfos.reserve(3);

	VkDescriptorImageInfo& albedoInfo = imageInfos.emplace_back(VkDescriptorImageInfo{});
	albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoInfo.imageView = m_AlbedoImage.GetImageView();
	albedoInfo.sampler = m_GBufferSampler;

	VkDescriptorImageInfo& normalInfo = imageInfos.emplace_back(VkDescriptorImageInfo{});
	normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalInfo.imageView = m_NormalImage.GetImageView();
	normalInfo.sampler = m_GBufferSampler;

	VkDescriptorImageInfo& materialInfo = imageInfos.emplace_back(VkDescriptorImageInfo{});
	materialInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	materialInfo.imageView = m_MaterialImage.GetImageView();
	materialInfo.sampler = m_GBufferSampler;

	std::array<VkWriteDescriptorSet, 3> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = m_GBufferDescriptorSet;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].descriptorCount = 1;
	writes[0].pImageInfo = &imageInfos[0];

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = m_GBufferDescriptorSet;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &imageInfos[1];

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = m_GBufferDescriptorSet;
	writes[2].dstBinding = 2;
	writes[2].dstArrayElement = 0;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].descriptorCount = 1;
	writes[2].pImageInfo = &imageInfos[2];

	vkUpdateDescriptorSets(m_Device.GetLogicalDevice(),
		static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void dae::Renderer::DeleteResources()
{
	m_DeletionQueue.Flush();
}

VkRenderingAttachmentInfo dae::Renderer::MakeColorAttachment(VkImageView view)
{
	VkRenderingAttachmentInfo attachment{};
	attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment.imageView = view;
	attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	return attachment;
}

VkRenderingAttachmentInfo dae::Renderer::MakeDepthAttachment(VkImageView view)
{
	VkRenderingAttachmentInfo attachment{};
	attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment.imageView = view;
	attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.clearValue.depthStencil.depth = 1.0f;
	return attachment;
}

void dae::Renderer::SetViewportScissor(VkCommandBuffer cmd)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_Swapchain.GetExtent().width);
	viewport.height = static_cast<float>(m_Swapchain.GetExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_Swapchain.GetExtent();
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}
