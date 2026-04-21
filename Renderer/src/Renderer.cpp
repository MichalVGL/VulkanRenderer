#include "Renderer.h"

#include "Frame.h"

void dae::Renderer::Init()
{
	if (m_IsInitialized)
	{
		DeleteResources();
	}

	//todo, add initialization code here
	//	extension, vulkan, ...0
	//todo, add code to create a size given externally

	//
	m_Window.Init({ 800, 600 }, "TODO, add changeable title and size");

	m_Device.Init(m_Window);
	m_DeletionQueue.PushDeleter(m_Device.GetDeletionFlush());

	m_Swapchain.Init(m_Device, m_Window);
	m_DeletionQueue.PushDeleter(m_Swapchain.GetDeletionFlush());

	CreateCommandPool();
	m_Frames.resize(m_MaxFramesInFlight);
	for (auto& frame : m_Frames)
	{
		frame.Init(m_Device, m_CommandPool);
		m_DeletionQueue.PushDeleter(frame.GetDeletionFlush());
	}

	m_IsInitialized = true;

}

void dae::Renderer::DrawFrame()
{
	Frame& currentFrame = m_Frames[m_CurrentFrame];

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

	currentFrame.EndCommands();

	currentFrame.Submit(m_Device);
	currentFrame.Present(m_Device, m_Swapchain, imageIndex); //todo, get image index from acquire next image

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
