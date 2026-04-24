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

	//todo, add initialization code here
	//	extension, vulkan, ...0
	//todo, add code to create a size given externally

	//

	// Device
	m_Device.Init(*m_pWindow);
	m_DeletionQueue.PushDeleter(m_Device.GetDeletionFlush());

	m_MaxFramesInFlight = g_DesiredMaxFramesInFlight;

	// Swapchain
	m_Swapchain.Init(m_Device, *m_pWindow, m_MaxFramesInFlight); //might change max frames in flight
	m_DeletionQueue.PushDeleter(m_Swapchain.GetDeletionFlush());

	// Pipeline
	PipelineInfo pipelineInfo{};
	pipelineInfo.vertexShaderFilePath = "resources/shaders/test.vert.spv";
	pipelineInfo.fragmentShaderFilePath = "resources/shaders/test.frag.spv";
	pipelineInfo.colorAttachmentFormat = m_Swapchain.GetImageFormat();
	pipelineInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	pipelineInfo.cullMode = VK_CULL_MODE_NONE; //todo, change
	pipelineInfo.useVertexPulling = false; //todo, change

	m_Pipeline.Init(m_Device, pipelineInfo);
	m_DeletionQueue.PushDeleter(m_Pipeline.GetDeletionFlush());

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

	image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	// begin rendering
	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = image.GetImageView();
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { {0, 0}, m_Swapchain.GetExtent() };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(commandBuffer, &renderingInfo);

	// set dynamic viewport and scissor
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_Swapchain.GetExtent().width);
	viewport.height = static_cast<float>(m_Swapchain.GetExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_Swapchain.GetExtent();
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// bind pipeline and draw
	m_Pipeline.Bind(commandBuffer);
	DrawScene(commandBuffer, scene);

	vkCmdEndRendering(commandBuffer);

	// transition to present
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
	//glm::mat4 view = glm::lookAt(
	//	glm::vec3(0.0f, 100.0f, 0.0f),  // above the scene
	//	glm::vec3(0.0f, 0.0f, 0.0f),    // looking at origin
	//	glm::vec3(0.0f, 0.0f, 1.0f)     // up is Z for top-down
	//);
	//
	//glm::mat4 proj = glm::perspective(
	//	glm::radians(90.0f),
	//	static_cast<float>(m_Swapchain.GetExtent().width) /
	//	static_cast<float>(m_Swapchain.GetExtent().height),
	//	0.1f, 10000.0f  // far plane very large for Sponza
	//);
	//proj[1][1] *= -1;
	//
	//glm::mat4 viewProj = proj * view;

	// 1. Get the View Matrix from the Scene's Camera
	glm::mat4 view = scene.GetCamera().GetCameraToWorld();

	// 2. Create the Projection Matrix
	// Note: We use the camera's FOV (in radians) and the swapchain's aspect ratio
	glm::mat4 proj = glm::perspective(
		glm::radians(scene.GetCamera().GetFOV()),
		static_cast<float>(m_Swapchain.GetExtent().width) / static_cast<float>(m_Swapchain.GetExtent().height),
		scene.GetCamera().m_Near, // Assuming you exposed m_Near/m_Far or have getters
		scene.GetCamera().m_Far
	);

	// 3. Flip the Y-axis for Vulkan (since glm was designed for OpenGL)
	proj[1][1] *= -1;

	// 4. Multiply them to get the View-Projection matrix
	glm::mat4 viewProj = proj * view;

	for (const auto& renderObject : scene.GetRenderObjects())
	{
		if (!renderObject.pMesh) continue;

		PushConstants push{};
		push.transform = viewProj * renderObject.transform;
		//push.vertexBufferAddress = renderObject.pMesh->GetVertexBufferAddress(m_Device);


		vkCmdPushConstants(cmd, m_Pipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(PushConstants), &push);

		VkBuffer vertexBuffers[] = { renderObject.pMesh->GetVertexBuffer().GetBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

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

void dae::Renderer::DeleteResources()
{
	m_DeletionQueue.Flush();
}
