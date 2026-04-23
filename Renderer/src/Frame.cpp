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
	vkResetFences(device.GetLogicalDevice(), 1, &m_InFlightFence);
}

VkCommandBuffer dae::Frame::BeginCommands()
{
	vkResetCommandBuffer(m_CommandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	//todo add rendering
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
	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = m_ImageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	waitSemaphoreInfo.value = 0;
	waitSemaphoreInfo.deviceIndex = 0;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = m_RenderFinishedSemaphore;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	signalSemaphoreInfo.value = 0;
	signalSemaphoreInfo.deviceIndex = 0;

	VkCommandBufferSubmitInfo cmdInfo{};
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdInfo.commandBuffer = m_CommandBuffer;
	cmdInfo.deviceMask = 0;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	if (vkQueueSubmit2(device.GetGraphicsQueue(), 1, &submitInfo, m_InFlightFence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit draw command buffer!");
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
