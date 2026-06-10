#include "Buffer.h"

#include "Device.h"

void dae::Buffer::Init(const Device& device, VkDeviceSize allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	m_DeletionQueue.Flush(); // Handle reinit
	m_Size = allocSize;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;
	//bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = memoryUsage;
	//allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	if (vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer!");
	}

	m_DeletionQueue.PushDeleter([this, allocator = device.GetAllocator()]()
		{
			vmaDestroyBuffer(allocator, m_Buffer, m_Allocation);
			m_Buffer = VK_NULL_HANDLE;
			m_Allocation = VK_NULL_HANDLE;
		});
}

std::function<void()> dae::Buffer::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

void* dae::Buffer::Map(const Device& device)
{
	void* data;
	vmaMapMemory(device.GetAllocator(), m_Allocation, &data);
	return data;
}

void dae::Buffer::Unmap(const Device& device)
{
	vmaUnmapMemory(device.GetAllocator(), m_Allocation);
}

void dae::Buffer::Unmap(VmaAllocator allocator)
{
	vmaUnmapMemory(allocator, m_Allocation);
}

VkDeviceAddress dae::Buffer::GetDeviceAddress(const Device& device) const
{
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = m_Buffer;
	return vkGetBufferDeviceAddress(device.GetLogicalDevice(), &addressInfo);
}
