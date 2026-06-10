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

	struct PushConstants
	{
		//todo, delete transform
		glm::mat4 transform{};        // 64 bytes
		uint64_t vertexBufferAddress{}; // 8 bytes
		uint32_t albedoTextureIndex;
		uint32_t normalTextureIndex;
		uint32_t metalicRoughnessTextureIndex;
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

	struct CPUPointLight {
		glm::vec3 pos;
		float lumen;
		float temperature;
	};

	struct CPUDirectionalLight {
		glm::vec3 dir;
		float lux;
		float temperature;
	};

	struct GPULight {
		glm::vec4 posDir;		// xyz = position or direction, w = type flag (<0 for directional, >0 for point)
		glm::vec4 color; 		// rgb = converted color, a = lumen or lux depending on type
	};
}


#endif // !GRAPHICSTYPES_H
