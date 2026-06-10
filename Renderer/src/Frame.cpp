#include "Frame.h"

#include "Device.h"
#include "Swapchain.h"

void dae::Frame::Init(const Device& device, VkCommandPool commandPool, VkDescriptorSetLayout sceneDataLayout, VkDescriptorSetLayout lightLayout)
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
	CreateSceneDataBuffer(device, sceneDataLayout);
	CreateLightBuffer(device, lightLayout);
}

void dae::Frame::WaitForFence(const Device& device)
{
	vkWaitForFences(device.GetLogicalDevice(), 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
}

void dae::Frame::ResetFence(const Device& device)
{
	vkResetFences(device.GetLogicalDevice(), 1, &m_InFlightFence);
}

void dae::Frame::UpdateSceneData(const SceneData& data)
{
	if (m_SceneDataMapped == nullptr)
	{
		std::cout << "[Frame.cpp] scene data buffer is not mapped!\n";
		return;
	}

	memcpy(m_SceneDataMapped, &data, sizeof(SceneData));
}

void dae::Frame::UpdateLights(const std::vector<GPULight>& lights)
{
	if (m_LightMapped == nullptr) return;

	uint32_t count = std::min(static_cast<uint32_t>(lights.size()), m_MaxLights);
	memcpy(m_LightMapped, lights.data(), count * sizeof(GPULight));
}

VkCommandBuffer dae::Frame::BeginCommands()
{
	vkResetCommandBuffer(m_CommandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

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
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
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

void dae::Frame::CreateSceneDataBuffer(const Device& device, VkDescriptorSetLayout sceneDataLayout)
{
	m_SceneDataBuffer.Init(device, sizeof(SceneData),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU);
	m_DeletionQueue.PushDeleter(m_SceneDataBuffer.GetDeletionFlush());

	m_SceneDataMapped = m_SceneDataBuffer.Map(device);
	m_DeletionQueue.PushDeleter([this, allocator = device.GetAllocator()]()
		{
			m_SceneDataBuffer.Unmap(allocator);
			m_SceneDataMapped = nullptr;
		});

	// descriptor pool
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(device.GetLogicalDevice(), &poolInfo, nullptr, &m_SceneDataDescriptorPool) != VK_SUCCESS)
		throw std::runtime_error("[Frame.cpp] failed to create scene data descriptor pool!");

	m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
		{
			vkDestroyDescriptorPool(logicalDevice, m_SceneDataDescriptorPool, nullptr);
			m_SceneDataDescriptorPool = VK_NULL_HANDLE;
		});

	// allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_SceneDataDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &sceneDataLayout;

	if (vkAllocateDescriptorSets(device.GetLogicalDevice(), &allocInfo, &m_SceneDataDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("[Frame.cpp] failed to allocate scene data descriptor set!");

	// write buffer into descriptor set
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = m_SceneDataBuffer.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(SceneData);

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_SceneDataDescriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(device.GetLogicalDevice(), 1, &write, 0, nullptr);
}

void dae::Frame::CreateLightBuffer(const Device& device, VkDescriptorSetLayout lightLayout)
{
	// create SSBO — sized for max lights
	m_LightBuffer.Init(device,
		sizeof(GPULight) * m_MaxLights,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	m_LightMapped = m_LightBuffer.Map(device);
	m_DeletionQueue.PushDeleter(m_LightBuffer.GetDeletionFlush());

	m_DeletionQueue.PushDeleter([this, allocator = device.GetAllocator()]()
		{
			m_LightBuffer.Unmap(allocator);
		});

	// descriptor pool
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(device.GetLogicalDevice(), &poolInfo, nullptr, &m_LightPool) != VK_SUCCESS)
		throw std::runtime_error("[Frame.cpp] failed to create light descriptor pool!");

	m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
		{
			vkDestroyDescriptorPool(logicalDevice, m_LightPool, nullptr);
			m_LightPool = VK_NULL_HANDLE;
		});

	// allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_LightPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &lightLayout;

	if (vkAllocateDescriptorSets(device.GetLogicalDevice(), &allocInfo, &m_LightDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("[Frame.cpp] failed to allocate light descriptor set!");

	// write buffer
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = m_LightBuffer.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(GPULight) * m_MaxLights;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_LightDescriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(device.GetLogicalDevice(), 1, &write, 0, nullptr);
}
