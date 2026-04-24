#ifndef SCENE_H
#define SCENE_H

#include "pch.h"
#include "Mesh.h"
#include "Texture.h"
#include "GraphicsTypes.h"
#include "Camera.h"

namespace dae
{
	class Renderer;
    class Scene final
    {
    public:
        Scene() = default;
        ~Scene() = default;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        void Init(const std::filesystem::path& filepath, const Renderer& renderer);

		void Update(float deltaTime, const InputHandler& input);

        const std::vector<RenderObject>& GetRenderObjects() const { return m_RenderObjects; }
		const Camera& GetCamera() const { return m_Camera; }

		std::function<void()> GetDeletionFlush();

    private:
        // Assimp forward declarations (or just hide behind void* if preferred, but easy enough to do internally)
        void ProcessNode(aiNode* aiNodePtr, const aiScene* aiScenePtr, const glm::mat4x4& parentTransform);
        Mesh* ProcessMesh(aiMesh* aiMeshPtr, const Device& device, VkCommandPool commandPool, VkQueue graphicsQueue);

        std::vector<std::unique_ptr<Mesh>> m_Meshes;
        std::vector<std::unique_ptr<Texture>> m_Textures;
        std::vector<std::unique_ptr<Material>> m_Materials;

        std::vector<RenderObject> m_RenderObjects;

        std::unique_ptr<Texture> m_DefaultTexture; // in case of a missing texture

		Camera m_Camera{};

        DeletionQueue m_DeletionQueue{};
    };
}

#endif // !SCENE_H
