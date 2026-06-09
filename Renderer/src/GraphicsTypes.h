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
		glm::vec2 uv{};
	};

	struct Material
	{
		uint32_t diffuseTextureIndex{};
	};

	struct RenderObject
	{
		Mesh* pMesh{ nullptr };
		Material* pMaterial{ nullptr };
		glm::mat4 transform{ 1.0f };
	};
}


#endif // !GRAPHICSTYPES_H
