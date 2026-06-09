#ifndef PIPELINE_H
#define PIPELINE_H

#include "pch.h"

#include "DeletionQueue.h"

namespace dae
{
	class Device;

	enum class DescriptorLayoutType
	{
		BindlessTextures,	// larget texture array
		GBuffer				// fixed GBuffer
	};

	struct PipelineInfo
	{
		std::filesystem::path vertexShaderFilePath{};
		std::filesystem::path fragmentShaderFilePath{};
		std::vector<VkFormat> colorAttachmentFormats{};
		VkBool32 depthTestEnable{ VK_FALSE };
		VkBool32 depthWriteEnable{ VK_FALSE };
		VkCompareOp depthCompareOp{ VK_COMPARE_OP_LESS };
		VkFormat depthAttachmentFormat{ VK_FORMAT_UNDEFINED };
		VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
		VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
		VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
		VkFrontFace frontFace{ VK_FRONT_FACE_COUNTER_CLOCKWISE };
		DescriptorLayoutType descriptorLayout{ DescriptorLayoutType::BindlessTextures };
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

		const VkPipeline& GetPipeline() const { return m_Pipeline; }
		const VkPipelineLayout& GetPipelineLayout() const { return m_PipelineLayout; }
		const PipelineInfo& GetPipelineInfo() const { return m_Info; }
		const VkDescriptorSetLayout& GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

	private:

		bool ValidatePipelineInfo(const PipelineInfo& info) const;

		std::vector<uint32_t> ReadShaderFile(const std::filesystem::path& filePath) const;
		VkShaderModule CreateShaderModule(std::vector<uint32_t> code, const Device& device) const;

		VkPipeline m_Pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
		PipelineInfo m_Info{};

		VkDescriptorSetLayout m_DescriptorSetLayout{ VK_NULL_HANDLE };

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !PIPELINE_H
