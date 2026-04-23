#include "Renderer.h"

#include "VulkanSetup.h"

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
	pipelineInfo.vertexShaderFilePath = "resources/shaders/triangle.vert.spv";
	pipelineInfo.fragmentShaderFilePath = "resources/shaders/triangle.frag.spv";
	pipelineInfo.colorAttachmentFormat = m_Swapchain.GetImageFormat();

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

void dae::Renderer::DrawFrame()
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
	vkCmdDraw(commandBuffer, 3, 1, 0, 0); // 3 vertices, hardcoded triangle

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
