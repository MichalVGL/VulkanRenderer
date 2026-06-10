#ifndef BUFFER_H
#define BUFFER_H

#include "pch.h"
#include <vk_mem_alloc.h>
#include "DeletionQueue.h"

namespace dae
{
	class Device;

	class Buffer final
	{
	public:
		Buffer() = default;
		~Buffer() = default;

		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		void Init(const Device& device, VkDeviceSize allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

		std::function<void()> GetDeletionFlush();

		// Helpers for mapping memory
		void* Map(const Device& device);
		void Unmap(const Device& device);
		void Unmap(VmaAllocator allocator);

		VkBuffer GetBuffer() const { return m_Buffer; }
		VkDeviceSize GetSize() const { return m_Size; }
		VkDeviceAddress GetDeviceAddress(const Device& device) const;

	private:
		VkBuffer m_Buffer{ VK_NULL_HANDLE };
		VmaAllocation m_Allocation{ VK_NULL_HANDLE };
		VkDeviceSize m_Size{ 0 };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif