#include "Pipeline.h"

#include <fstream>

#include "Device.h"
#include "GraphicsTypes.h"

void dae::Pipeline::Init(const Device& device, const PipelineInfo& info)
{
	//verify input
	if (!device.IsInitialized())
	{
		std::cerr << "[Pipeline.cpp] device given in initialization is not initialized!\n";
		throw std::invalid_argument("Device is not initialized!");

	}
	else if (!ValidatePipelineInfo(info))
	{
		std::cerr << "[Pipeline.cpp] pipeline info given in initialization is not valid!\n";
		throw std::invalid_argument("PipelineInfo is not valid!");
	}

	m_DeletionQueue.Flush(); // in case of reinitialization
	m_Info = info;

	// load and create shader modules
	std::vector<uint32_t> vertCode = ReadShaderFile(info.vertexShaderFilePath);
	std::vector<uint32_t> fragCode = ReadShaderFile(info.fragmentShaderFilePath);

	VkShaderModule vertModule = CreateShaderModule(vertCode, device);
	VkShaderModule fragModule = CreateShaderModule(fragCode, device);

	// shader stage creation
	VkPipelineShaderStageCreateInfo vertStage{};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = vertModule;
	vertStage.pName = "main";

	VkPipelineShaderStageCreateInfo fragStage{};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = fragModule;
	fragStage.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{ vertStage, fragStage };

	// dynamic states
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// vertex input, empty for vertex pulling
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = info.topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// viewport and scissor, dynamic
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	//rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = info.polygonMode;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = info.cullMode;
	rasterizer.frontFace = info.frontFace;
	rasterizer.depthBiasEnable = VK_FALSE;

	// multisampling, disabled
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	//color blending, no blending
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(
		info.colorAttachmentFormats.size());

	for (auto& attachment : colorBlendAttachments)
	{
		attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		attachment.blendEnable = VK_FALSE;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
	colorBlending.pAttachments = colorBlendAttachments.data();

	// depth stencil
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = info.depthTestEnable;
	depthStencil.depthWriteEnable = info.depthWriteEnable;
	depthStencil.depthCompareOp = info.depthCompareOp;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// define flags, only used for bindless textures with variable descriptor count
	VkDescriptorBindingFlags bindingFlags =
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
	flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagsInfo.bindingCount = 1;
	flagsInfo.pBindingFlags = &bindingFlags;

	std::vector<VkDescriptorSetLayoutBinding> bindings{};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};

	switch (info.texturesDescriptorLayout)
	{
	case TexturesDescriptorLayoutType::BindlessTextures:
		{
			VkDescriptorSetLayoutBinding& binding = bindings.emplace_back();
			binding.binding = 0;
			binding.descriptorCount = 1000; // Max textures in the array
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			layoutInfo.pNext = &flagsInfo; // Enable variable descriptor count for bindless textures
		}
		break;
	case TexturesDescriptorLayoutType::GBuffer:
		bindings.reserve(4);
		for (int i = 0; i < 4; i++) // albedo, normal, material (mettalic/roughness), depth
		{
			VkDescriptorSetLayoutBinding& binding = bindings.emplace_back();
			binding.binding = i;
			binding.descriptorCount = 1;
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		layoutInfo.pNext = nullptr; // No variable descriptor count needed for fixed GBuffer
		break;
	case TexturesDescriptorLayoutType::SingleImage:
		{
			VkDescriptorSetLayoutBinding& binding = bindings.emplace_back();
			binding.binding = 0;
			binding.descriptorCount = 1; //HDR image
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			layoutInfo.pNext = nullptr;
		}
		break;
	default:
		throw std::runtime_error("[Pipeline.cpp] unknown descriptor layout type!");
		break;
	}

	// fill in descriptor set layout
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device.GetLogicalDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]() {
		vkDestroyDescriptorSetLayout(logicalDevice, m_DescriptorSetLayout, nullptr);
		m_DescriptorSetLayout = VK_NULL_HANDLE;
		});

	// layouts
	std::vector<VkDescriptorSetLayout> setLayouts = {
		m_DescriptorSetLayout
	};

	for (auto& additionalLayout : info.additionalSetLayouts)
		setLayouts.push_back(additionalLayout);

	// pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();

	if (info.pushConstantRange.size > 0)
	{
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &info.pushConstantRange;
	}

	if (vkCreatePipelineLayout(device.GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("[Pipeline.cpp] failed to create pipeline layout!");
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroyPipelineLayout(logicalDevice, m_PipelineLayout, nullptr);
				m_PipelineLayout = VK_NULL_HANDLE;
			});
	}

	// dynamic rendering info
	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = static_cast<uint32_t>(info.colorAttachmentFormats.size());
	renderingInfo.pColorAttachmentFormats = info.colorAttachmentFormats.data();
	renderingInfo.depthAttachmentFormat = info.depthAttachmentFormat;

	// pipeline creation
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = &renderingInfo;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState = &multisampling;
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	pipelineCreateInfo.pColorBlendState = &colorBlending;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.layout = m_PipelineLayout;
	pipelineCreateInfo.renderPass = VK_NULL_HANDLE; // not needed with dynamic rendering

	if (vkCreateGraphicsPipelines(device.GetLogicalDevice(), VK_NULL_HANDLE, 1,
		&pipelineCreateInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("[Pipeline.cpp] failed to create graphics pipeline!");
	}
	else
	{
		m_DeletionQueue.PushDeleter([this, logicalDevice = device.GetLogicalDevice()]()
			{
				vkDestroyPipeline(logicalDevice, m_Pipeline, nullptr);
				m_Pipeline = VK_NULL_HANDLE;
			});
	}

	// shader modules can be destroyed after pipeline creation
	vkDestroyShaderModule(device.GetLogicalDevice(), vertModule, nullptr);
	vkDestroyShaderModule(device.GetLogicalDevice(), fragModule, nullptr);
}

std::function<void()> dae::Pipeline::GetDeletionFlush()
{
	return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
	);
}

void dae::Pipeline::Bind(VkCommandBuffer cmd) const
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

bool dae::Pipeline::IsInitialized() const
{
	return m_Pipeline != VK_NULL_HANDLE
		&& m_PipelineLayout != VK_NULL_HANDLE;
}

bool dae::Pipeline::ValidatePipelineInfo(const PipelineInfo& info) const
{
	// check vertex shader
	if (!std::filesystem::exists(info.vertexShaderFilePath))
	{
		std::cerr << "[Pipeline.cpp] Vertex shader file does not exist: "
			<< info.vertexShaderFilePath << "\n";
		return false;
	}

	if (info.vertexShaderFilePath.extension() != ".spv")
	{
		std::cerr << "[Pipeline.cpp] Vertex shader file is not a .spv file: "
			<< info.vertexShaderFilePath << "\n";
		return false;
	}

	// check fragment shader
	if (!info.fragmentShaderFilePath.empty()) //valid for depth prepass
	{
		if (!std::filesystem::exists(info.fragmentShaderFilePath))
		{
			std::cerr << "[Pipeline.cpp] Fragment shader file does not exist: " << info.fragmentShaderFilePath << "\n";
			return false;
		}
		if (info.fragmentShaderFilePath.extension() != ".spv")
		{
			std::cerr << "[Pipeline.cpp] Fragment shader file is not a .spv: " << info.fragmentShaderFilePath << "\n";
			return false;
		}
	}

	return true;
}

std::vector<uint32_t> dae::Pipeline::ReadShaderFile(const std::filesystem::path& filePath) const
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("[Pipeline.cpp] failed to open shader file: " + filePath.string());

	size_t fileSize = static_cast<size_t>(file.tellg());

	if (fileSize == 0)
		throw std::runtime_error("[Pipeline.cpp] shader file is empty: " + filePath.string());

	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

	if (!file)
		throw std::runtime_error("[Pipeline.cpp] failed to read shader file: " + filePath.string());

	file.close();

	return buffer;
}

VkShaderModule dae::Pipeline::CreateShaderModule(std::vector<uint32_t> code, const Device& device) const
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device.GetLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule; //make sure to delete this shader externally
}
