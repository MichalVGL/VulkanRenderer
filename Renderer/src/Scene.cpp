#include "Scene.h"

#include "MathHelpers.h"
#include "Renderer.h"

void dae::Scene::Init(const std::filesystem::path& filepath, const Renderer& renderer)
{
    Assimp::Importer importer;
    const aiScene* pScene = importer.ReadFile(filepath.string(),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
                                                                
    if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
    {
        throw std::runtime_error("ERROR::ASSIMP::" + std::string(importer.GetErrorString()));
    }

	const Device& device = renderer.GetDevice();
	VkCommandPool commandPool = renderer.GetCommandPool();

	//create a backup white texture
    m_DefaultTexture = std::make_unique<Texture>();
    unsigned char whitePixel[4] = { 255, 255, 255, 255 }; // RGBA White

    m_DefaultTexture->Init(device, 1, 1, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1);

    m_DefaultTexture->LoadPixels(device, commandPool, whitePixel, 1, 1);
	m_DeletionQueue.PushDeleter(m_DefaultTexture->GetDeletionFlush());

    //process materials and textures
    m_Materials.reserve(pScene->mNumMaterials);
    for (unsigned int i = 0; i < pScene->mNumMaterials; i++)
    {
        aiMaterial* pAiMaterial = pScene->mMaterials[i];
        auto pNewMaterial = std::make_unique<Material>();

        // Check if this material has a diffuse (base color) texture
        aiString texturePath;
        if (pAiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
        {
            // Construct the full path (textures are usually relative to the 3D model file)
            std::filesystem::path modelDirectory = filepath.parent_path();
            std::filesystem::path fullTexturePath = modelDirectory / texturePath.C_Str();

            // Create and load the texture
            auto pNewTexture = std::make_unique<Texture>();

            // Assuming you added the LoadFromFile method we discussed earlier:
            pNewTexture->InitFromFile(device, commandPool, fullTexturePath);
			m_DeletionQueue.PushDeleter(pNewTexture->GetDeletionFlush());

            pNewMaterial->pDiffuseTexture = pNewTexture.get();
            m_Textures.push_back(std::move(pNewTexture));
        }
        else
        {
            // Fallback
			std::cout << "Material " << i << " has no diffuse texture. Using default white texture." << std::endl;
            pNewMaterial->pDiffuseTexture = m_DefaultTexture.get();
        }

        m_Materials.push_back(std::move(pNewMaterial));
    }

    //process all meshes
    m_Meshes.reserve(pScene->mNumMeshes);
    for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
    {
        aiMesh* pMesh = pScene->mMeshes[i];

        m_Meshes.push_back(std::unique_ptr<Mesh>(ProcessMesh(pMesh, device, commandPool, device.GetGraphicsQueue())));
        m_DeletionQueue.PushDeleter(m_Meshes.back()->GetDeletionFlush());

        // Define the mesh's single primitive mapping
        Primitive prim{};
        prim.firstIndex = 0;
        prim.indexCount = pMesh->mNumFaces * 3;
        prim.materialIndex = pMesh->mMaterialIndex;
        m_Meshes.back()->AddPrimitive(prim);
    }

    //process all nodes
    ProcessNode(pScene->mRootNode, pScene, glm::mat4x4(1.0f));

    m_DeletionQueue.PushDeleter([logicalDevice = device.GetLogicalDevice()]()
        {
			//wait for the GPU to finish before destroying the scene resources
            vkDeviceWaitIdle(logicalDevice);
		});
}

void dae::Scene::Update(float deltaTime, const InputHandler& input)
{
	m_Camera.Update(deltaTime, input);
}

std::function<void()> dae::Scene::GetDeletionFlush()
{
    return std::function<void()>(
		std::bind(&DeletionQueue::Flush, &m_DeletionQueue)
    );
}

void dae::Scene::ProcessNode(aiNode* aiNodePtr, const aiScene* aiScenePtr, const glm::mat4x4& parentTransform)
{
    glm::mat4 localTransform = ConvertMatrixToGLMFormat(aiNodePtr->mTransformation);
    glm::mat4 worldTransform = parentTransform * localTransform;

	//create world objects for all meshes of this node
    for (unsigned int i = 0; i < aiNodePtr->mNumMeshes; i++)
    {
        unsigned int meshIndex = aiNodePtr->mMeshes[i];

        RenderObject obj{};
        obj.pMesh = m_Meshes[meshIndex].get();
        obj.transform = worldTransform;

        m_RenderObjects.push_back(obj);
    }

    // recursive call for all children
    for (unsigned int i = 0; i < aiNodePtr->mNumChildren; i++)
    {
        ProcessNode(aiNodePtr->mChildren[i], aiScenePtr, worldTransform);
    }
}

dae::Mesh* dae::Scene::ProcessMesh(aiMesh* aiMeshPtr, const Device& device, VkCommandPool commandPool, VkQueue graphicsQueue)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(aiMeshPtr->mNumVertices);

    // get vertices
    for (unsigned int i = 0; i < aiMeshPtr->mNumVertices; i++)
    {
        Vertex vertex{};

        // Position
        vertex.position = { aiMeshPtr->mVertices[i].x, aiMeshPtr->mVertices[i].y, aiMeshPtr->mVertices[i].z };

        // Normal
        if (aiMeshPtr->HasNormals())
            vertex.normal = { aiMeshPtr->mNormals[i].x, aiMeshPtr->mNormals[i].y, aiMeshPtr->mNormals[i].z };
        else
            vertex.normal = { 0.0f, 1.0f, 0.0f }; // Default facing up

        // Texture Coordinates
        if (aiMeshPtr->mTextureCoords[0])
            vertex.uv = { aiMeshPtr->mTextureCoords[0][i].x, aiMeshPtr->mTextureCoords[0][i].y };
        else
            vertex.uv = { 0.0f, 0.0f }; // Default origin

        vertices.push_back(vertex);
    }

    // get indices
    indices.reserve(static_cast<size_t>(aiMeshPtr->mNumFaces) * 3);
    for (unsigned int i = 0; i < aiMeshPtr->mNumFaces; i++)
    {
        aiFace face = aiMeshPtr->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // create mesh
    Mesh* pNewMesh = new Mesh();
    pNewMesh->Init(device, commandPool, vertices, indices);

    return pNewMesh;
}
