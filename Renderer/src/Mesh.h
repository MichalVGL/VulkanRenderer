#ifndef MESH_H
#define MESH_H

#include "pch.h"

#include "DeletionQueue.h"
#include "Buffer.h"
#include "Device.h"
#include "GraphicsTypes.h"

namespace dae
{
	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t indexCount;
		int materialIndex; // Index of scene material
	};

	class Mesh final
	{
	public:

		Mesh() = default;
		~Mesh() = default;

		Mesh(const Mesh&) = delete;
		Mesh(Mesh&&) = delete;
		Mesh& operator=(const Mesh&) = delete;
		Mesh& operator=(Mesh&&) = delete;

		void Init(const Device& device, VkCommandPool commandPool,
			const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

		std::function<void()> GetDeletionFlush();

		const Buffer& GetVertexBuffer() const { return m_VertexBuffer; }
		const Buffer& GetIndexBuffer() const { return m_IndexBuffer; }
		const std::vector<Primitive>& GetPrimitives() const { return m_Primitives; }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		VkDeviceAddress GetVertexBufferAddress(const Device& device) const;
		uint32_t GetIndexCount() const;

		// Let the glTF loader add primitives to this mesh
		void AddPrimitive(const Primitive& primitive) { m_Primitives.push_back(primitive); }

	private:
		Buffer m_VertexBuffer;
		Buffer m_IndexBuffer;

		std::vector<Vertex> m_Vertices; //cpu copy

		std::vector<Primitive> m_Primitives;

		DeletionQueue m_DeletionQueue;
	};
}

#endif // !MESH_H
