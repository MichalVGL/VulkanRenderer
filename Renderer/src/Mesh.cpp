#include "Mesh.h"

void dae::Mesh::Init(const Device& device, VkCommandPool commandPool, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    m_DeletionQueue.Flush(); // clear any old data if re-initializing

	m_Vertices = vertices;
  
    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

    if (vertexBufferSize == 0 || indexBufferSize == 0) return;

    // create buffers
    Buffer vertexStaging;
    vertexStaging.Init(device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    Buffer indexStaging;
    indexStaging.Init(device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	// copy data to staging buffers
    void* vertexData = vertexStaging.Map(device);
    memcpy(vertexData, vertices.data(), static_cast<size_t>(vertexBufferSize));
    vertexStaging.Unmap(device);

    void* indexData = indexStaging.Map(device);
    memcpy(indexData, indices.data(), static_cast<size_t>(indexBufferSize));
    indexStaging.Unmap(device);

	// create GPU local buffers
    m_VertexBuffer.Init(device, vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_DeletionQueue.PushDeleter(m_VertexBuffer.GetDeletionFlush());

    m_IndexBuffer.Init(device, indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_DeletionQueue.PushDeleter(m_IndexBuffer.GetDeletionFlush());

	// create temp command buffer for copy operations
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.GetLogicalDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

	// copy to GPU local buffers
    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(commandBuffer, vertexStaging.GetBuffer(), m_VertexBuffer.GetBuffer(), 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(commandBuffer, indexStaging.GetBuffer(), m_IndexBuffer.GetBuffer(), 1, &indexCopyRegion);

	// end and submit command buffer
    vkEndCommandBuffer(commandBuffer);

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = commandBuffer;
    cmdInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdInfo;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.signalSemaphoreInfoCount = 0;

    vkQueueSubmit2(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);

    // wait for completion
    vkQueueWaitIdle(device.GetGraphicsQueue());
    vkFreeCommandBuffers(device.GetLogicalDevice(), commandPool, 1, &commandBuffer);
}

std::function<void()> dae::Mesh::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

VkDeviceAddress dae::Mesh::GetVertexBufferAddress(const Device& device) const
{
    return m_VertexBuffer.GetDeviceAddress(device);
}

uint32_t dae::Mesh::GetIndexCount() const
{
    uint32_t total = 0;
    for (const auto& prim : m_Primitives)
        total += prim.indexCount;
    return total;
}
