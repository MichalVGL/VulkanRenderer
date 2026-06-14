#include "Light.h"
#include "Renderer.h"
#include "Device.h"
#include "Scene.h"

void dae::Light::Init(const Renderer& renderer, const Scene& scene)
{
	const AABB sceneAABB = scene.GetSceneAABB();

	if (m_Type == LightType::Point) //no depth image supported for point lights yet
		return;

	const Device& device = renderer.GetDevice();
	VkCommandPool commandPool = renderer.GetCommandPool();
	DeletionQueue functionDeletionQueue{};

	m_DepthImage.Init(device, m_DepthResolution, m_DepthResolution, VK_FORMAT_D32_SFLOAT
		, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
		, VK_IMAGE_ASPECT_DEPTH_BIT);
	m_DeletionQueue.PushDeleter(m_DepthImage.GetDeletionFlush());

	//==============================
	// Setup variables

	glm::vec3 sceneCenter = (sceneAABB.min + sceneAABB.max) * 0.5f;
	glm::vec3 lightDirection = glm::normalize(m_PosOrDir);

	//generate 8 AABB corners
	std::vector<glm::vec3> corners = {
		{sceneAABB.min.x, sceneAABB.min.y, sceneAABB.min.z},
		{sceneAABB.max.x, sceneAABB.min.y, sceneAABB.min.z},
		{sceneAABB.min.x, sceneAABB.max.y, sceneAABB.min.z},
		{sceneAABB.max.x, sceneAABB.max.y, sceneAABB.min.z},
		{sceneAABB.min.x, sceneAABB.min.y, sceneAABB.max.z},
		{sceneAABB.max.x, sceneAABB.min.y, sceneAABB.max.z},
		{sceneAABB.min.x, sceneAABB.max.y, sceneAABB.max.z},
		{sceneAABB.max.x, sceneAABB.max.y, sceneAABB.max.z},
	};

	//project corners onto light dir
	float minProj = FLT_MAX;
	float maxProj = -FLT_MAX;
	for (const auto& corner : corners) {
		float proj = glm::dot(corner, lightDirection);
		minProj = std::min(minProj, proj);
		maxProj = std::max(maxProj, proj);
	}

	//calculate distance and position, even though direction light doesnt have a position
	float distance = maxProj - glm::dot(sceneCenter, lightDirection);
	glm::vec3 lightPos = sceneCenter - lightDirection * distance;

	//calculate safe up vector
	glm::vec3 up = glm::abs(glm::dot(lightDirection, glm::vec3(0, 1, 0))) > 0.99f
		? glm::vec3(0, 0, 1)
		: glm::vec3(0, 1, 0);

	m_View = glm::lookAt(lightPos, sceneCenter, up);

	// find min max in view space
	glm::vec3 minLS(FLT_MAX);
	glm::vec3 maxLS(-FLT_MAX);
	for (const auto& corner : corners) {
		glm::vec3 tc = glm::vec3(m_View * glm::vec4(corner, 1.f));
		minLS = glm::min(minLS, tc);
		maxLS = glm::max(maxLS, tc);
	}

	float nearZ = 0.0f;
	float farZ = maxLS.z - minLS.z;
	m_Proj = glm::ortho(
		minLS.x, maxLS.x,
		minLS.y, maxLS.y,
		nearZ, farZ );
	m_Proj[1][1] *= -1.f;
	//==============================
	// Pipeline

	VkPushConstantRange pushConstants{}; //constains view and projection matrices
	pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstants.offset = 0;
	pushConstants.size = sizeof(ShadowPushConstants);

	PipelineInfo pipelineInfo{};
	pipelineInfo.vertexShaderFilePath = "resources/shaders/DirLightDepth.vert.spv";
	pipelineInfo.fragmentShaderFilePath = "resources/shaders/DirLightDepth.frag.spv";
	pipelineInfo.colorAttachmentFormats = {};
	pipelineInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineInfo.depthAttachmentFormat = m_DepthImage.GetFormat();
	pipelineInfo.depthTestEnable = VK_TRUE;
	pipelineInfo.depthWriteEnable = VK_TRUE;
	pipelineInfo.pushConstantRange = pushConstants;
	pipelineInfo.texturesDescriptorLayout = TexturesDescriptorLayoutType::None;
	pipelineInfo.additionalSetLayouts = {};
	pipelineInfo.depthBiasEnable = true;
	pipelineInfo.depthBiasConstantFactor = 1.25f;
	pipelineInfo.depthBiasClamp = 0.f;
	pipelineInfo.depthBiasSlopeFactor = 1.75f;

	Pipeline pipeline{};
	pipeline.Init(device, pipelineInfo);
	functionDeletionQueue.PushDeleter(pipeline.GetDeletionFlush());

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

	//transition depth to be written to
	m_DepthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

	// begin depth-only rendering
	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = m_DepthImage.GetImageView();
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue.depthStencil.depth = 1.0f;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { {0, 0}, {m_DepthResolution, m_DepthResolution} };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 0;
	renderingInfo.pColorAttachments = nullptr;
	renderingInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(commandBuffer, &renderingInfo);

	// viewport + scissor sized to shadow map, not swapchain
	VkViewport viewport{};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = static_cast<float>(m_DepthResolution);
	viewport.height = static_cast<float>(m_DepthResolution);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{ {0,0}, {m_DepthResolution, m_DepthResolution} };
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	pipeline.Bind(commandBuffer);

	// push light space matrix
	glm::mat4 lightSpaceMat = m_Proj * m_View;

	for (const auto& renderObject : scene.GetRenderObjects())
	{
		if (!renderObject.pMesh) continue;

		ShadowPushConstants push{};
		push.lightSpaceTransform = lightSpaceMat * renderObject.transform;
		push.vertexBufferAddress = renderObject.pMesh->GetVertexBufferAddress(device);

		vkCmdPushConstants(commandBuffer, pipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push);

		vkCmdBindIndexBuffer(commandBuffer,
			renderObject.pMesh->GetIndexBuffer().GetBuffer(),
			0, VK_INDEX_TYPE_UINT32);

		for (const auto& primitive : renderObject.pMesh->GetPrimitives()) {
			vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1,
				primitive.firstIndex, 0, 0);
		}
	}

	vkCmdEndRendering(commandBuffer);

	// transition to shader read, stays here permanently
	m_DepthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_READ_BIT);

	vkEndCommandBuffer(commandBuffer);

	// submit and wait
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device.GetGraphicsQueue());
}

dae::GPULight dae::Light::GetGPULightData() const
{
	GPULight gpu{};
	switch (m_Type)
	{
	case LightType::Point:
		gpu.posDir = glm::vec4(m_PosOrDir, POINT_W_VALUE);       // w > 0 = point light
		break;
	case LightType::Directional:
		gpu.posDir = glm::vec4(m_PosOrDir, DIRECTIONAL_W_VALUE);      // w < 0 = directional light
		break;
	default:
		throw std::runtime_error("[Light] Unknown light type");
	}

	gpu.color = glm::vec4(KelvinToRgb(temperature), m_LumenOrLux);

	gpu.lightView = m_View;
	gpu.lightProj = m_Proj;
	gpu.depthIndex = m_DepthIndex;

	return gpu;
}

std::function<void()> dae::Light::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}
