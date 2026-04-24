#ifndef GRAPHICSTYPES_H
#define GRAPHICSTYPES_H

#include "pch.h"


namespace dae
{
	class Mesh;
	class Texture;

	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;

		//todo, switch to modern version
		static VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription binding{};
			binding.binding = 0;
			binding.stride = sizeof(Vertex);
			binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return binding;
		}

		static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attributes{};
			attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) };
			attributes[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) };
			attributes[2] = { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, uv) };
			return attributes;
		}
	};

	struct Material
	{
		Texture* pDiffuseTexture{ nullptr };
	};

	struct RenderObject
	{
		Mesh* pMesh{ nullptr };
		Material* pMaterial{ nullptr };
		glm::mat4 transform{ 1.0f };
	};
}


#endif // !GRAPHICSTYPES_H
