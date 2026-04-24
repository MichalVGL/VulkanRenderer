#ifndef PIPELINE_H
#define PIPELINE_H

#include "pch.h"

#include "DeletionQueue.h"

namespace dae
{
	class Device;

	struct PipelineInfo
	{
		std::filesystem::path vertexShaderFilePath{};
		std::filesystem::path fragmentShaderFilePath{};
		VkFormat colorAttachmentFormat{ VK_FORMAT_UNDEFINED };
		VkFormat depthAttachmentFormat{ VK_FORMAT_UNDEFINED };
		VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
		VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
		VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
		VkFrontFace frontFace{ VK_FRONT_FACE_COUNTER_CLOCKWISE };
		bool useVertexPulling{ false };

		bool HasMinimalRequiredInfo() const
		{
			return !vertexShaderFilePath.empty()
				&& !fragmentShaderFilePath.empty()
				&& colorAttachmentFormat != VK_FORMAT_UNDEFINED;
		}
	};

	class Pipeline final
	{
	public:

		Pipeline() = default;
		~Pipeline() = default;

		Pipeline(const Pipeline&) = delete;
		Pipeline(Pipeline&&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;
		Pipeline& operator=(Pipeline&&) = delete;

		void Init(const Device& device, const PipelineInfo& info);
		std::function<void()> GetDeletionFlush();
		bool IsInitialized() const;

		void Bind(VkCommandBuffer cmd) const;

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
		const PipelineInfo& GetPipelineInfo() const { return m_Info; }

	private:

		bool ValidatePipelineInfo(const PipelineInfo& info) const;

		std::vector<uint32_t> ReadShaderFile(const std::filesystem::path& filePath) const;
		VkShaderModule CreateShaderModule(std::vector<uint32_t> code, const Device& device) const;

		VkPipeline m_Pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
		PipelineInfo m_Info{};

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !PIPELINE_H
