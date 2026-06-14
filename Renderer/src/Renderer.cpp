#include "Renderer.h"

#include "VulkanSetup.h"
#include "Scene.h"
#include "CubeMap.h"

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

	CreateLayoutBindings();

	CreateSamplers();

	InitializeImages();

	InitializePipelines();

	//Setup frames and command pool
	CreateCommandPool();
	m_Frames.resize(m_MaxFramesInFlight);

	for (int i = 0; i < m_MaxFramesInFlight; ++i)
		m_Frames[i] = std::make_unique<Frame>();

	for (auto& frame : m_Frames)
	{
		frame->Init(m_Device, m_CommandPool, m_SceneDataLayout, m_LightLayout);
		m_DeletionQueue.PushDeleter(frame->GetDeletionFlush());
	}

	// initialize descriptor sets for GBuffer and HDR image (used in lighting and blitting pass)
	InitializeGBufferDescriptors();
	InitializeHDRImage();
	//initialize the skybox
	InitializeIBL();


	m_DeletionQueue.PushDeleter([this]()
		{
			vkDeviceWaitIdle(m_Device.GetLogicalDevice());
		});

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

	ImageRef& swapImage = m_Swapchain.GetImage(imageIndex);

	//==============================
	// Depth PrePass

	// Depth (read and write)
	m_DepthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

	// depth attachment
	VkRenderingAttachmentInfo depthAttachment = MakeDepthAttachment(m_DepthImage.GetImageView(), true, true);

	VkRenderingInfo depthPrePassRenderingInfo{};
	depthPrePassRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	depthPrePassRenderingInfo.renderArea = { {0, 0}, m_Swapchain.GetExtent() };
	depthPrePassRenderingInfo.layerCount = 1;
	depthPrePassRenderingInfo.colorAttachmentCount = 0;
	depthPrePassRenderingInfo.pColorAttachments = nullptr;
	depthPrePassRenderingInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(commandBuffer, &depthPrePassRenderingInfo);

	// set dynamic viewport and scissor
	SetViewportScissor(commandBuffer);

	m_DepthPrePassPipeline.Bind(commandBuffer);

	DrawScene(commandBuffer, scene, currentFrame, m_DepthPrePassPipeline);
	vkCmdEndRendering(commandBuffer);

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

	// Depth (read only)
	m_DepthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

	// setup GBuffer attachments
	std::array<VkRenderingAttachmentInfo, 3> gBufferAttachments{};
	gBufferAttachments[0] = MakeColorAttachment(m_AlbedoImage.GetImageView());
	gBufferAttachments[1] = MakeColorAttachment(m_NormalImage.GetImageView());
	gBufferAttachments[2] = MakeColorAttachment(m_MaterialImage.GetImageView());

	depthAttachment = MakeDepthAttachment(m_DepthImage.GetImageView(), false, true); //dont clear depth again, but keep it for lighting pass

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

	m_GBufferPipeline.Bind(commandBuffer);

	//draw scene to gbuffer
	DrawScene(commandBuffer, scene, currentFrame, m_GBufferPipeline);
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

	// Depth (read only)
	m_DepthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);

	//HDR image (output)
	m_HDRImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	//Update the lights buffer with the current scene lights
	std::vector<GPULight> gpuLights{};
	gpuLights.reserve(scene.GetLights().size());
	for (const auto& light : scene.GetLights()) {
		gpuLights.push_back(light->GetGPULightData());
	}
	currentFrame.UpdateLights(gpuLights);

	VkRenderingAttachmentInfo hdrAttachment = MakeColorAttachment(m_HDRImage.GetImageView());

	depthAttachment = MakeDepthAttachment(m_DepthImage.GetImageView(), false, false, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL); //dont clear depth and dont store it

	VkRenderingInfo lightingRenderingInfo{};
	lightingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	lightingRenderingInfo.renderArea = { {0, 0}, m_Swapchain.GetExtent() };
	lightingRenderingInfo.layerCount = 1;
	lightingRenderingInfo.colorAttachmentCount = 1;
	lightingRenderingInfo.pColorAttachments = &hdrAttachment;
	lightingRenderingInfo.pDepthAttachment = &depthAttachment; //todo, maybe remove

	vkCmdBeginRendering(commandBuffer, &lightingRenderingInfo);
	SetViewportScissor(commandBuffer);
	m_LightingPipeline.Bind(commandBuffer);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_LightingPipeline.GetPipelineLayout(),
		0, 1, &m_GBufferDescriptorSet, 0, nullptr);
	VkDescriptorSet sceneDataSet = currentFrame.GetSceneDataDescriptorSet();
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_LightingPipeline.GetPipelineLayout(),
		1, 1, &sceneDataSet, 0, nullptr);
	VkDescriptorSet lightSet = currentFrame.GetLightDescriptorSet();
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_LightingPipeline.GetPipelineLayout(),
		2, 1, &lightSet, 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_LightingPipeline.GetPipelineLayout(),
		3, 1, &m_IBLDiffuseDescriptorSet, 0, nullptr);
	VkDescriptorSet shadowSet = scene.GetShadowDescriptorSet(); // ← new
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_LightingPipeline.GetPipelineLayout(),
		4, 1, &shadowSet, 0, nullptr);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0); // fullscreen triangle, hardcoded in shader
	vkCmdEndRendering(commandBuffer);

	//==============================
	// Blitting Pass

	// HDR image (read)
	m_HDRImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);

	// SwapImage (output)
	swapImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	//push constants info
	BlitPushConstants blitPushConstants{};
	blitPushConstants.exposure = scene.GetCamera().GetExposure();

	VkRenderingAttachmentInfo swapImageAttachment = MakeColorAttachment(swapImage.GetImageView());

	VkRenderingInfo blitRenderingInfo{};
	blitRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	blitRenderingInfo.renderArea = { {0, 0}, m_Swapchain.GetExtent() };
	blitRenderingInfo.layerCount = 1;
	blitRenderingInfo.colorAttachmentCount = 1;
	blitRenderingInfo.pColorAttachments = &swapImageAttachment;
	blitRenderingInfo.pDepthAttachment = nullptr;

	vkCmdBeginRendering(commandBuffer, &blitRenderingInfo);
	SetViewportScissor(commandBuffer);
	m_BlittingPipeline.Bind(commandBuffer);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_BlittingPipeline.GetPipelineLayout(), 0, 1, &m_HDRDescriptorSet, 0, nullptr);
	vkCmdPushConstants(commandBuffer, m_BlittingPipeline.GetPipelineLayout(),
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(BlitPushConstants), &blitPushConstants);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRendering(commandBuffer);

	//==============================
	// Transition to present

	swapImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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

void dae::Renderer::DrawScene(VkCommandBuffer cmd, const Scene& scene, Frame& currentFrame, const Pipeline& pipeline)
{
	//todo, move the scenedata building to the drawframe, now it calculates it multiple times due to deferred
	// build scene data
	glm::mat4 view = scene.GetCamera().GetCameraToWorld();
	glm::mat4 proj = glm::perspective(
		glm::radians(scene.GetCamera().GetFOV()),
		static_cast<float>(m_Swapchain.GetExtent().width) / static_cast<float>(m_Swapchain.GetExtent().height),
		scene.GetCamera().m_Near,
		scene.GetCamera().m_Far
	);
	proj[1][1] *= -1;

	SceneData sceneData{}; //todo, delete view and proj from scene data, they are not used in the shaders, only the viewproj is used
	sceneData.view = view;
	sceneData.proj = proj;
	sceneData.viewProj = proj * view;
	sceneData.cameraPos = glm::vec4(scene.GetCamera().GetOrigin(), 1.0f);
	sceneData.lightCount = static_cast<uint32_t>(scene.GetLights().size());

	currentFrame.UpdateSceneData(sceneData);

	// set 0 = global descriptor set (textures)
	VkDescriptorSet globalSet = scene.GetTexturesDescriptorSet();
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS
		, m_GBufferPipeline.GetPipelineLayout(),
		0, 1, &globalSet, 0, nullptr);

	// set 1 = scene data UBO
	VkDescriptorSet sceneDataSet = currentFrame.GetSceneDataDescriptorSet();
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_GBufferPipeline.GetPipelineLayout(),
		1, 1, &sceneDataSet, 0, nullptr);

	for (const auto& renderObject : scene.GetRenderObjects())
	{
		if (!renderObject.pMesh
			|| !renderObject.pMaterial)
		{
			continue;
		}

		MeshPushConstants push{};
		push.transform = renderObject.transform;
		push.vertexBufferAddress = renderObject.pMesh->GetVertexBufferAddress(m_Device);
		push.albedoTextureIndex = renderObject.pMaterial->albedoTextureIndex;
		push.normalTextureIndex = renderObject.pMaterial->normalTextureIndex;
		push.metalicRoughnessTextureIndex = renderObject.pMaterial->metalicRoughnessTextureIndex;

		vkCmdPushConstants(cmd, pipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(MeshPushConstants), &push);

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

void dae::Renderer::CreateLayoutBindings()
{
	//=========================
	// scene data

	// Create scene data layout that every frame uses
	VkDescriptorSetLayoutBinding sceneDataBinding{};
	sceneDataBinding.binding = 0;
	sceneDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	sceneDataBinding.descriptorCount = 1;
	sceneDataBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo sceneDataLayoutInfo{};
	sceneDataLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	sceneDataLayoutInfo.bindingCount = 1;
	sceneDataLayoutInfo.pBindings = &sceneDataBinding;

	vkCreateDescriptorSetLayout(m_Device.GetLogicalDevice(),
		&sceneDataLayoutInfo, nullptr, &m_SceneDataLayout);

	m_DeletionQueue.PushDeleter([this]()
		{
			vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(), m_SceneDataLayout, nullptr);
			m_SceneDataLayout = VK_NULL_HANDLE;
		});

	//=========================
	// lights

	// lights from every frame
	VkDescriptorSetLayoutBinding lightBinding{};
	lightBinding.binding = 0;
	lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightBinding.descriptorCount = 1;
	lightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo lightLayoutInfo{};
	lightLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lightLayoutInfo.bindingCount = 1;
	lightLayoutInfo.pBindings = &lightBinding;

	// create both descriptor sets

	vkCreateDescriptorSetLayout(m_Device.GetLogicalDevice(),
		&lightLayoutInfo, nullptr, &m_LightLayout);

	m_DeletionQueue.PushDeleter([this]()
		{
			vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(),
				m_LightLayout, nullptr);
			m_LightLayout = VK_NULL_HANDLE;
		});

	//=========================
	// light shadows

	{
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = Scene::MAX_LIGHTS;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;

		// create both descriptor sets

		vkCreateDescriptorSetLayout(m_Device.GetLogicalDevice(),
			&layoutInfo, nullptr, &m_ShadowLayout);

		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(),
					m_ShadowLayout, nullptr);
				m_ShadowLayout = VK_NULL_HANDLE;
			});
	}

	//=========================
	// cubemaps 

	//Skybox
	{
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;

		vkCreateDescriptorSetLayout(m_Device.GetLogicalDevice(),
			&layoutInfo, nullptr, &m_SkyboxLayout);

		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(),
					m_SkyboxLayout, nullptr);
				m_SkyboxLayout = VK_NULL_HANDLE;
			});
	}

	//IBL diffuse
	{
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;

		vkCreateDescriptorSetLayout(m_Device.GetLogicalDevice(),
			&layoutInfo, nullptr, &m_IBLDiffuseLayout);

		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(),
					m_IBLDiffuseLayout, nullptr);
				m_IBLDiffuseLayout = VK_NULL_HANDLE;
			});
	}
}

void dae::Renderer::CreateSamplers()
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

	//=========================
	// Shadow sampler
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		//samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		//samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;      
		samplerInfo.compareEnable = VK_TRUE;                               
		samplerInfo.compareOp = VK_COMPARE_OP_LESS;
		//samplerInfo.minLod = 0.0f;
		//samplerInfo.maxLod = 1.0f;
		vkCreateSampler(m_Device.GetLogicalDevice(), &samplerInfo, nullptr, &m_ShadowSampler);
		m_DeletionQueue.PushDeleter([this]()
			{
				vkDestroySampler(m_Device.GetLogicalDevice(), m_ShadowSampler, nullptr);
				m_ShadowSampler = VK_NULL_HANDLE;
			});
	}
}

void dae::Renderer::InitializeImages()
{
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
		VK_FORMAT_R16G16B16A16_UNORM,
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
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	m_DeletionQueue.PushDeleter(m_DepthImage.GetDeletionFlush());

	//==============================
	// HDR Image

	m_HDRImage.Init(m_Device, extent.width, extent.height,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, 1);
	m_DeletionQueue.PushDeleter(m_HDRImage.GetDeletionFlush());
}

void dae::Renderer::InitializePipelines()
{
	//==============================
	// Push constant ranges

	VkPushConstantRange meshPushConstantsRange{};
	meshPushConstantsRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	meshPushConstantsRange.offset = 0;
	meshPushConstantsRange.size = sizeof(MeshPushConstants);

	VkPushConstantRange blitPushConstantsRange{};
	blitPushConstantsRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	blitPushConstantsRange.offset = 0;
	blitPushConstantsRange.size = sizeof(BlitPushConstants);

	//==============================
	// Deferred Pipelines 

	// Depth PrePass Pipeline
	PipelineInfo depthPrePassPipelineInfo{};
	depthPrePassPipelineInfo.vertexShaderFilePath = "resources/shaders/DepthPrePass.vert.spv";
	depthPrePassPipelineInfo.fragmentShaderFilePath = "resources/shaders/DepthPrePass.frag.spv";
	depthPrePassPipelineInfo.colorAttachmentFormats = {};
	depthPrePassPipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	depthPrePassPipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	depthPrePassPipelineInfo.depthAttachmentFormat = m_DepthImage.GetFormat();
	depthPrePassPipelineInfo.depthTestEnable = VK_TRUE;
	depthPrePassPipelineInfo.depthWriteEnable = VK_TRUE;
	depthPrePassPipelineInfo.pushConstantRange = meshPushConstantsRange;
	depthPrePassPipelineInfo.texturesDescriptorLayout = TexturesDescriptorLayoutType::BindlessTextures; //still needed to check alpha
	depthPrePassPipelineInfo.additionalSetLayouts = { m_SceneDataLayout };

	m_DepthPrePassPipeline.Init(m_Device, depthPrePassPipelineInfo);
	m_DeletionQueue.PushDeleter(m_DepthPrePassPipeline.GetDeletionFlush());

	// GBuffer Pipeline
	PipelineInfo gBufferPipelineInfo{};
	gBufferPipelineInfo.vertexShaderFilePath = "resources/shaders/GBuffer.vert.spv";
	gBufferPipelineInfo.fragmentShaderFilePath = "resources/shaders/GBuffer.frag.spv";
	gBufferPipelineInfo.colorAttachmentFormats = { m_AlbedoImage.GetFormat(), m_NormalImage.GetFormat(), m_MaterialImage.GetFormat() };
	gBufferPipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	gBufferPipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	gBufferPipelineInfo.depthAttachmentFormat = m_DepthImage.GetFormat();
	gBufferPipelineInfo.depthTestEnable = VK_TRUE;
	gBufferPipelineInfo.depthWriteEnable = VK_FALSE;
	gBufferPipelineInfo.depthCompareOp = VK_COMPARE_OP_EQUAL;
	gBufferPipelineInfo.pushConstantRange = meshPushConstantsRange;
	gBufferPipelineInfo.texturesDescriptorLayout = TexturesDescriptorLayoutType::BindlessTextures;
	gBufferPipelineInfo.additionalSetLayouts = { m_SceneDataLayout };

	m_GBufferPipeline.Init(m_Device, gBufferPipelineInfo);
	m_DeletionQueue.PushDeleter(m_GBufferPipeline.GetDeletionFlush());

	// Lighting Pipeline
	PipelineInfo lightingPipelineInfo{};
	lightingPipelineInfo.vertexShaderFilePath = "resources/shaders/Lighting.vert.spv";
	lightingPipelineInfo.fragmentShaderFilePath = "resources/shaders/Lighting.frag.spv";
	lightingPipelineInfo.colorAttachmentFormats = { m_HDRImage.GetFormat() };
	lightingPipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	lightingPipelineInfo.cullMode = VK_CULL_MODE_NONE;
	lightingPipelineInfo.depthAttachmentFormat = m_DepthImage.GetFormat(); //todo,, maybe nullptr?
	lightingPipelineInfo.depthTestEnable = VK_FALSE;
	lightingPipelineInfo.depthWriteEnable = VK_FALSE;
	lightingPipelineInfo.texturesDescriptorLayout = TexturesDescriptorLayoutType::GBuffer;
	lightingPipelineInfo.additionalSetLayouts = { m_SceneDataLayout, m_LightLayout, m_IBLDiffuseLayout, m_ShadowLayout };

	m_LightingPipeline.Init(m_Device, lightingPipelineInfo);
	m_DeletionQueue.PushDeleter(m_LightingPipeline.GetDeletionFlush());

	// Blitting Pipeline
	PipelineInfo blittingPipelineInfo{};
	blittingPipelineInfo.vertexShaderFilePath = "resources/shaders/Blit.vert.spv";
	blittingPipelineInfo.fragmentShaderFilePath = "resources/shaders/Blit.frag.spv";
	blittingPipelineInfo.colorAttachmentFormats = { m_Swapchain.GetImageFormat() };
	blittingPipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	blittingPipelineInfo.cullMode = VK_CULL_MODE_NONE;
	blittingPipelineInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	blittingPipelineInfo.depthTestEnable = VK_FALSE;
	blittingPipelineInfo.depthWriteEnable = VK_FALSE;
	blittingPipelineInfo.pushConstantRange = blitPushConstantsRange;
	blittingPipelineInfo.texturesDescriptorLayout = TexturesDescriptorLayoutType::SingleImage;
	blittingPipelineInfo.additionalSetLayouts = {};

	m_BlittingPipeline.Init(m_Device, blittingPipelineInfo);
	m_DeletionQueue.PushDeleter(m_BlittingPipeline.GetDeletionFlush());
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

void dae::Renderer::InitializeGBufferDescriptors()
{
	//===================================
	// Descriptor Pool

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 4; // albedo, normal, material, depth

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolInfo, nullptr, &m_GBufferDescriptorPool) != VK_SUCCESS)
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

	if (vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocInfo, &m_GBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("[Renderer] failed to allocate GBuffer descriptor set!");
	}

	// fill in the descriptor sets

	std::vector<VkDescriptorImageInfo> imageInfos{};
	imageInfos.reserve(4);

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

	VkDescriptorImageInfo& depthInfo = imageInfos.emplace_back(VkDescriptorImageInfo{});
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	depthInfo.imageView = m_DepthImage.GetImageView();
	depthInfo.sampler = m_GBufferSampler;

	std::array<VkWriteDescriptorSet, 4> writes{};
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

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = m_GBufferDescriptorSet;
	writes[3].dstBinding = 3;
	writes[3].dstArrayElement = 0;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[3].descriptorCount = 1;
	writes[3].pImageInfo = &imageInfos[3];

	vkUpdateDescriptorSets(m_Device.GetLogicalDevice(),
		static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void dae::Renderer::InitializeHDRImage()
{
	//===================================
	//HDR Sampler 

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;

	if (vkCreateSampler(m_Device.GetLogicalDevice(), &samplerInfo, nullptr, &m_HDRSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("[Renderer] failed to create HDR sampler!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = m_Device.GetLogicalDevice()]()
			{
				vkDestroySampler(logicalDevice, m_HDRSampler, nullptr);
				m_HDRSampler = VK_NULL_HANDLE;
			});
	}

	//===================================
	// Descriptor Pool

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolInfo, nullptr, &m_HDRDescriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("[Renderer] failed to create HDR descriptor pool!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = m_Device.GetLogicalDevice()]()
			{
				vkDestroyDescriptorPool(logicalDevice, m_HDRDescriptorPool, nullptr);
				m_HDRDescriptorPool = VK_NULL_HANDLE;
			});
	}

	//===================================
	// Descriptor sets

	// Allocate the descriptor set

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_HDRDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_BlittingPipeline.GetDescriptorSetLayout();

	if (vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocInfo, &m_HDRDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("[Renderer] failed to allocate HDR descriptor set!");
	}

	// fill in the descriptor sets

	VkDescriptorImageInfo imageInfo;

	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = m_HDRImage.GetImageView();
	imageInfo.sampler = m_HDRSampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_HDRDescriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_Device.GetLogicalDevice(),
		1, &write, 0, nullptr);
}

void dae::Renderer::InitializeIBL()
{
	// Sky Box

	DeletionQueue functionDeletionQueue{};

	//todo, delete temp
	Texture tempTexture{};
	tempTexture.InitFromFile(m_Device, m_CommandPool, "Resources/hdri/circus_arena_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT);
	functionDeletionQueue.PushDeleter(tempTexture.GetDeletionFlush());

	m_SkyBox.Init(m_Device, 1024, VK_FORMAT_R32G32B32A32_SFLOAT
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		, VK_IMAGE_ASPECT_COLOR_BIT);
	m_SkyBox.RenderFromImage(m_Device, m_CommandPool, tempTexture.GetImage().GetImageView(), tempTexture.GetImage().GetCurrentLayout()
		, "resources/shaders/UnitCube.vert.spv", "resources/shaders/CubeMapConvert.frag.spv");
	m_DeletionQueue.PushDeleter(m_SkyBox.GetDeletionFlush());

	{
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolInfo, nullptr, &m_SkyBoxDescriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("[Renderer] failed to create SkyBox descriptor pool!");
		}
		else
		{
			m_DeletionQueue.PushDeleter([this, logicalDevice = m_Device.GetLogicalDevice()]()
				{
					vkDestroyDescriptorPool(logicalDevice, m_SkyBoxDescriptorPool, nullptr);
					m_SkyBoxDescriptorPool = VK_NULL_HANDLE;
				});
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_SkyBoxDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_SkyboxLayout;

		if (vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocInfo, &m_SkyBoxDescriptorSet) != VK_SUCCESS)
		{
			throw std::runtime_error("[Renderer] failed to allocate SkyBox descriptor set!");
		}

		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = m_SkyBox.GetSampler();
		imageInfo.imageView = m_SkyBox.GetImageView();
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_SkyBoxDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_Device.GetLogicalDevice(), 1, &write, 0, nullptr);
	}

	// IBL Diffuse

	m_IBLDiffuse.Init(m_Device, 64, VK_FORMAT_R32G32B32A32_SFLOAT
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	m_IBLDiffuse.RenderFromImage(m_Device, m_CommandPool, m_SkyBox.GetImageView(), m_SkyBox.GetCurrentLayout()
		, "resources/shaders/UnitCube.vert.spv", "resources/shaders/IBLDiffuse.frag.spv");
	m_DeletionQueue.PushDeleter(m_IBLDiffuse.GetDeletionFlush());
	{
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolInfo, nullptr, &m_IBLDiffuseDescriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("[Renderer] failed to create IBL Diffuse descriptor pool!");
		}
		else
		{
			m_DeletionQueue.PushDeleter([this, logicalDevice = m_Device.GetLogicalDevice()]()
				{
					vkDestroyDescriptorPool(logicalDevice, m_IBLDiffuseDescriptorPool, nullptr);
					m_IBLDiffuseDescriptorPool = VK_NULL_HANDLE;
				});
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_IBLDiffuseDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_IBLDiffuseLayout;

		if (vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocInfo, &m_IBLDiffuseDescriptorSet) != VK_SUCCESS)
		{
			throw std::runtime_error("[Renderer] failed to allocate IBL Diffuse descriptor set!");
		}

		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = m_IBLDiffuse.GetSampler();
		imageInfo.imageView = m_IBLDiffuse.GetImageView();
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_IBLDiffuseDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_Device.GetLogicalDevice(), 1, &write, 0, nullptr);
	}
}

void dae::Renderer::DeleteResources()
{
	m_DeletionQueue.Flush();
}

void dae::Renderer::RecreateSwapchainResources()
{
	/*
	// Handle minimization - wait until window has size again
	int width = 0, height = 0;
	m_pWindow->GetFramebufferSize(&width, &height);
	while (width == 0 || height == 0) {
		m_pWindow->GetFramebufferSize(&width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_Device.GetLogicalDevice());

	// Destroy only extent-dependent resources, in reverse order
	vkDestroyDescriptorPool(m_Device.GetLogicalDevice(), m_GBufferDescriptorPool, nullptr);
	vkDestroyDescriptorPool(m_Device.GetLogicalDevice(), m_HDRDescriptorPool, nullptr);
	m_GBufferDescriptorPool = VK_NULL_HANDLE;
	m_HDRDescriptorPool = VK_NULL_HANDLE;

	m_HDRImage.Destroy();
	m_DepthImage.Destroy();
	m_MaterialImage.Destroy();
	m_NormalImage.Destroy();
	m_AlbedoImage.Destroy();

	m_Swapchain.Destroy(m_Device);

	// Recreate
	m_Swapchain.Init(m_Device, *m_pWindow, m_MaxFramesInFlight);
	InitializeImages();       // uses new extent
	InitializeGBufferDescriptors();
	InitializeHDRImage();
	*/
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

VkRenderingAttachmentInfo dae::Renderer::MakeDepthAttachment(VkImageView view, bool clearImageOnLoad, bool storeImage, VkImageLayout layout)
{
	VkRenderingAttachmentInfo attachment{};
	attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment.imageView = view;
	attachment.imageLayout = layout;
	attachment.loadOp = clearImageOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment.storeOp = storeImage ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
