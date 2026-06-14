#ifndef GRAPHICSTYPES_H
#define GRAPHICSTYPES_H

#include "pch.h"


namespace dae
{
	class Mesh;
	class Texture;

	struct Vertex
	{
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec4 tangent{}; // w = 1.0f for right-handed, -1.0f for left-handed
		glm::vec2 uv{};
	};

	struct Material
	{
		uint32_t albedoTextureIndex{};
		uint32_t normalTextureIndex{};
		uint32_t metalicRoughnessTextureIndex{};
	};

	struct RenderObject
	{
		Mesh* pMesh{ nullptr };
		Material* pMaterial{ nullptr };
		glm::mat4 transform{ 1.0f };
	};

	struct MeshPushConstants //(mesh info) 
	{
		//todo, rename transform
		glm::mat4 transform{};        // 64 bytes
		uint64_t vertexBufferAddress{}; // 8 bytes
		uint32_t albedoTextureIndex{};
		uint32_t normalTextureIndex{};
		uint32_t metalicRoughnessTextureIndex{};
	};

	struct BlitPushConstants
	{
		float exposure{};
	};

	struct SceneData
	{
		glm::mat4 view{};
		glm::mat4 proj{};
		glm::mat4 viewProj{};
		glm::vec4 cameraPos{}; // vec4 for alignment (w unused)
		uint32_t lightCount{};
		glm::vec3 padding{};
	};
}


#endif // !GRAPHICSTYPES_H
