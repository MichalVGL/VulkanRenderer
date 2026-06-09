#ifndef TEXTURE_H
#define TEXTURE_H

#include "pch.h"

#include "Image.h"

namespace dae
{
	class Texture final
	{
	public:
		Texture() = default;
		~Texture() = default;

		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;
		Texture(Texture&&) = delete;
		Texture& operator=(Texture&&) = delete;

		void Init(const Device& device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels = 1);
		void InitFromFile(const Device& device, VkCommandPool commandPool, const std::filesystem::path& filepath, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

		void LoadPixels(const Device& device, VkCommandPool commandPool, void* pixels, uint32_t width, uint32_t height);

		std::function<void()> GetDeletionFlush();

		const Image& GetImage() const { return m_Image; }
		VkSampler GetSampler() const { return m_Sampler; }

	private:
		Image m_Image{};
		VkSampler m_Sampler{ VK_NULL_HANDLE };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !TEXTURE_H