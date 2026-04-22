#include "Renderer.h"

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

	m_Device.Init(*m_pWindow);
	m_DeletionQueue.PushDeleter(m_Device.GetDeletionFlush());

	m_Swapchain.Init(m_Device, *m_pWindow);
	m_DeletionQueue.PushDeleter(m_Swapchain.GetDeletionFlush());

	CreateCommandPool();
	m_Frames.resize(m_MaxFramesInFlight);

	for (int i = 0; i < m_MaxFramesInFlight; ++i)
		m_Frames[i] = std::make_unique<Frame>();

	for (auto& frame : m_Frames)
	{
		frame->Init(m_Device, m_CommandPool);
		m_DeletionQueue.PushDeleter(frame->GetDeletionFlush());
	}

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

	VkCommandBuffer commandBuffer = currentFrame.BeginCommands();

	//todo, add command buffer recording here
	// 1. transition image to transfer dst optimal
	VkImageMemoryBarrier barrierToClear{};
	barrierToClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrierToClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrierToClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrierToClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToClear.image = m_Swapchain.GetImage(imageIndex);
	barrierToClear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrierToClear.subresourceRange.baseMipLevel = 0;
	barrierToClear.subresourceRange.levelCount = 1;
	barrierToClear.subresourceRange.baseArrayLayer = 0;
	barrierToClear.subresourceRange.layerCount = 1;
	barrierToClear.srcAccessMask = 0;
	barrierToClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrierToClear);

	// 2. clear
	VkClearColorValue clearColor = { { 0.0f, 1.0f, 1.0f, 1.0f } };
	VkImageSubresourceRange clearRange{};
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel = 0;
	clearRange.levelCount = 1;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount = 1;

	vkCmdClearColorImage(commandBuffer,
		m_Swapchain.GetImage(imageIndex),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clearColor, 1, &clearRange);

	// 3. transition image to present src
	VkImageMemoryBarrier barrierToPresent{};
	barrierToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrierToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrierToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToPresent.image = m_Swapchain.GetImage(imageIndex);
	barrierToPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrierToPresent.subresourceRange.baseMipLevel = 0;
	barrierToPresent.subresourceRange.levelCount = 1;
	barrierToPresent.subresourceRange.baseArrayLayer = 0;
	barrierToPresent.subresourceRange.layerCount = 1;
	barrierToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrierToPresent.dstAccessMask = 0;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrierToPresent);

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
