#ifndef SCENE_H
#define SCENE_H

#include "pch.h"
#include "Mesh.h"
#include "Texture.h"
#include "GraphicsTypes.h"
#include "Camera.h"
#include "LightUtils.h"
#include "CubeMap.h"
#include "Light.h"

namespace dae
{
    struct AABB {
        glm::vec3 min{ FLT_MAX };
        glm::vec3 max{ -FLT_MAX };

        void Expand(const glm::vec3& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
    };

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
        Camera& GetCamera() { return m_Camera; }
        VkDescriptorSet& GetTexturesDescriptorSet() { return m_TextureDescriptorSet; }
        const VkDescriptorSet& GetTexturesDescriptorSet() const { return m_TextureDescriptorSet; }
        VkDescriptorSet& GetShadowDescriptorSet() { return m_ShadowDescriptorSet; }
        const VkDescriptorSet& GetShadowDescriptorSet() const { return m_ShadowDescriptorSet; }
		const std::vector<std::unique_ptr<Light>>& GetLights() const { return m_Lights; }
		AABB GetSceneAABB() const { return m_SceneAABB; }

		std::function<void()> GetDeletionFlush();

        static constexpr int MAX_MATERIALS = 1000;
        static constexpr int MAX_LIGHTS = 100;

    private:

		void ProcessMaterials(const aiScene* aiScenePtr, const Renderer& renderer, const std::filesystem::path& rootFilePath);
        Mesh* ProcessMesh(aiMesh* aiMeshPtr, const aiMatrix4x4& rootTransform, const Device& device, VkCommandPool commandPool, VkQueue graphicsQueue);
        void ProcessNode(aiNode* aiNodePtr, const aiScene* aiScenePtr, const glm::mat4x4& parentTransform);

		void LoadMaterials(const Renderer& renderer);
		void LoadLights(const Renderer& renderer);
		void CreateDefaultTextures(const Device& device, VkCommandPool commandPool);

        std::vector<std::unique_ptr<Mesh>> m_Meshes{};
        std::vector<std::unique_ptr<Texture>> m_Textures{};
        std::vector<std::unique_ptr<Material>> m_Materials{};
        std::vector<RenderObject> m_RenderObjects{};

		AABB m_SceneAABB{};

        VkDescriptorPool m_TextureDescriptorPool{ VK_NULL_HANDLE };
        VkDescriptorSet m_TextureDescriptorSet{ VK_NULL_HANDLE };

		Camera m_Camera{};

        std::vector<std::unique_ptr<Light>> m_Lights{};

        VkDescriptorPool m_ShadowDescriptorPool{ VK_NULL_HANDLE };
        VkDescriptorSet m_ShadowDescriptorSet{ VK_NULL_HANDLE };

        DeletionQueue m_DeletionQueue{};

        static constexpr int DEFAULT_DIFFUSE_TEXTURE_INDEX = 0;
        static constexpr int DEFAULT_NORMAL_TEXTURE_INDEX = 1;
        static constexpr int DEFAULT_METALIC_ROUGHNESS_TEXTURE_INDEX = 2;

        static constexpr float UNIT_SCALE = 100.f; //sponza is at 0.008 scale. I assume that is in cm, so scale to m
    };
}

#endif // !SCENE_H
