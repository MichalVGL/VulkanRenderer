#include "Frame.h"

#include "Device.h"
#include "Swapchain.h"

void dae::Frame::Init(const Device& device, VkCommandPool commandPool)
{
	// verify input
	if (device.IsInitialized() == false)
	{
		std::cerr << "[Frame.cpp] device given in initialization is not initialized!\n";
		throw std::invalid_argument("Device is not initialized!");
	}

	if (commandPool == VK_NULL_HANDLE)
	{
		std::cerr << "[Frame.cpp] command pool given in initialization is not valid!\n";
		throw std::invalid_argument("Command pool is not valid!");
	}

	// in case of reinitialization, clear previous resources
	m_DeletionQueue.Flush();

	CreateCommandBuffer(device, commandPool);
	CreateSyncObjects(device);
}

void dae::Frame::WaitForFence(const Device& device)
{
	vkWaitForFences(device.GetLogicalDevice(), 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
}

void dae::Frame::ResetFence(const Device& device)
{

}

VkCommandBuffer dae::Frame::BeginCommands()
{
	vkResetCommandBuffer(m_CommandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	//todo start here
	//vkCmdBeginRendering(m_CommandBuffer, ...);

	return m_CommandBuffer;
}

void dae::Frame::EndCommands()
{
	if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void dae::Frame::Submit(const Device& device)
{
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	//todo, maybe add this??
	//VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphore;
	//submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_CommandBuffer;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphore;

	if (vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, m_InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}
}

void dae::Frame::Present(const Device& device, const Swapchain& swapchain, uint32_t imageIndex)
{
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphore;

	VkSwapchainKHR swapChains[] = { swapchain.GetSwapChain() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	VkResult result = vkQueuePresentKHR(device.GetPresentQueue(), &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)// || framebufferResized) 
	{
		//todo, add resizing handling here
		//framebufferResized = false;
		//recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}

std::function<void()> dae::Frame::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

void dae::Frame::CreateCommandBuffer(const Device& device, VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device.GetLogicalDevice(), &allocInfo, &m_CommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, commandPool, logicalDevice = device.GetLogicalDevice()]()
			{
				vkFreeCommandBuffers(logicalDevice, commandPool, 1, &m_CommandBuffer);
				m_CommandBuffer = VK_NULL_HANDLE;
			});
	}
}

void dae::Frame::CreateSyncObjects(const Device& device)
{
	VkDevice logicalDevice = device.GetLogicalDevice();

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_InFlightFence) != VK_SUCCESS) 
	{
		throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice]()
			{
				vkDestroySemaphore(logicalDevice, m_ImageAvailableSemaphore, nullptr);
				m_ImageAvailableSemaphore = VK_NULL_HANDLE;
				vkDestroySemaphore(logicalDevice, m_RenderFinishedSemaphore, nullptr);
				m_RenderFinishedSemaphore = VK_NULL_HANDLE;
				vkDestroyFence(logicalDevice, m_InFlightFence, nullptr);
				m_InFlightFence = VK_NULL_HANDLE;
			});
	}
}
