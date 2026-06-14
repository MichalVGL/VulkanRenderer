#include "Scene.h"

#include "MathHelpers.h"
#include "Renderer.h"

void dae::Scene::Init(const std::filesystem::path& filepath, const Renderer& renderer)
{
	Assimp::Importer importer;
	const aiScene* pScene = importer.ReadFile(filepath.string(),
		aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

	if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
	{
		throw std::runtime_error("ERROR::ASSIMP::" + std::string(importer.GetErrorString()));
	}

	const Device& device = renderer.GetDevice();
	VkCommandPool commandPool = renderer.GetCommandPool();

	CreateDefaultTextures(device, commandPool);

	ProcessMaterials(pScene, renderer, filepath);
	//process all meshes
	m_Meshes.reserve(pScene->mNumMeshes);
	const aiMatrix4x4 rootTransform = pScene->mRootNode->mTransformation;
	for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
	{
		aiMesh* pMesh = pScene->mMeshes[i];

		m_Meshes.push_back(std::unique_ptr<Mesh>(ProcessMesh(pMesh, rootTransform, device, commandPool, device.GetGraphicsQueue())));
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

	LoadMaterials(renderer); //load to the gpu
	
	SetupCamera();
	CreateLights();

	LoadLights(renderer); //load to the gpu

	// Add a deleter to wait for the GPU to finish rendering before destroying scene resources
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

void dae::Scene::SetupCamera()
{
	//adjust camera, indoor settings from slides
	m_Camera.SetAperture(1.4f);
	m_Camera.SetShutterSpeed(1.f / 60.f);
	m_Camera.SetISO(1600.f);
	m_Camera.SetOrigin({ 0.f, 1.f, 0.f });
	m_Camera.SetYaw(glm::radians(-90.f));
}

void dae::Scene::CreateLights()
{
	auto* pLight = m_Lights.emplace_back(
		Light::CreateDirectional(glm::normalize(glm::vec3(0.2f, -1.f, 0.4f)), 4096)
	).get();
	pLight->SetLumenOrLux(20.f);
	pLight->SetTemperature(3700.f);
	m_DeletionQueue.PushDeleter(pLight->GetDeletionFlush());

	pLight = m_Lights.emplace_back(
		Light::CreatePoint(glm::vec3(7.f, 1.f, 0.f), 1024) //no shadows implemented
	).get();
	pLight->SetLumenOrLux(400.f);
	pLight->SetTemperature(2700.f);

	m_DeletionQueue.PushDeleter(pLight->GetDeletionFlush());
	pLight = m_Lights.emplace_back(
		Light::CreatePoint(glm::vec3(-7.f, 1.f, 0.f), 1024) //no shadows implemented
	).get();
	pLight->SetLumenOrLux(400.f);
	pLight->SetTemperature(7700.f);
	m_DeletionQueue.PushDeleter(pLight->GetDeletionFlush());

	pLight = m_Lights.emplace_back(
		Light::CreatePoint(glm::vec3(0.f, 1.f, 0.f), 1024) //no shadows implemented
	).get();
	pLight->SetLumenOrLux(200.f);
	pLight->SetTemperature(1000.f);
	m_DeletionQueue.PushDeleter(pLight->GetDeletionFlush());
}

void dae::Scene::ProcessMaterials(const aiScene* aiScenePtr, const Renderer& renderer, const std::filesystem::path& rootFilePath)
{
	const Device& device = renderer.GetDevice();
	VkCommandPool commandPool = renderer.GetCommandPool();

	//process materials and textures
	m_Materials.reserve(aiScenePtr->mNumMaterials);
	for (unsigned int i = 0; i < aiScenePtr->mNumMaterials; i++)
	{
		aiMaterial* pAiMaterial = aiScenePtr->mMaterials[i];
		auto pNewMaterial = std::make_unique<Material>();

		aiString texturePath;

		//=================================
		// Diffuse texture
		if (pAiMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == AI_SUCCESS)
		{
			// Construct the full path (textures are usually relative to the 3D model file)
			std::filesystem::path modelDirectory = rootFilePath.parent_path();
			std::filesystem::path fullTexturePath = modelDirectory / texturePath.C_Str();

			// Create and load the texture
			auto pNewTexture = std::make_unique<Texture>();

			pNewTexture->InitFromFile(device, commandPool, fullTexturePath);
			m_DeletionQueue.PushDeleter(pNewTexture->GetDeletionFlush());

			pNewMaterial->albedoTextureIndex = static_cast<uint32_t>(m_Textures.size());
			m_Textures.push_back(std::move(pNewTexture));
		}
		else // Fallback
		{
#ifdef DEBUG
			std::cout << "Material " << i << " has no diffuse texture. Using default white texture." << std::endl;
#endif // DEBUG
			pNewMaterial->albedoTextureIndex = DEFAULT_DIFFUSE_TEXTURE_INDEX;
		}

		//=================================
		// Normal texture
		if (pAiMaterial->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS)
		{
			// Construct the full path (textures are usually relative to the 3D model file)
			std::filesystem::path modelDirectory = rootFilePath.parent_path();
			std::filesystem::path fullTexturePath = modelDirectory / texturePath.C_Str();

			// Create and load the texture
			auto pNewTexture = std::make_unique<Texture>();

			pNewTexture->InitFromFile(device, commandPool, fullTexturePath, VK_FORMAT_R16G16B16A16_UNORM); //todo, to VK_FORMAT_R16G16B16A16_UNORM if slider are correct
			m_DeletionQueue.PushDeleter(pNewTexture->GetDeletionFlush());

			pNewMaterial->normalTextureIndex = static_cast<uint32_t>(m_Textures.size());
			m_Textures.push_back(std::move(pNewTexture));
		}
		else // Fallback
		{
#ifdef DEBUG
			std::cout << "Material " << i << " has no normal texture. Using default texture." << std::endl;
#endif // DEBUG
			pNewMaterial->normalTextureIndex = DEFAULT_NORMAL_TEXTURE_INDEX;
		}

		//=================================
		// Metalness-Roughness texture
		if (pAiMaterial->GetTexture(aiTextureType_METALNESS, 0, &texturePath) == AI_SUCCESS)
		{
			// Construct the full path (textures are usually relative to the 3D model file)
			std::filesystem::path modelDirectory = rootFilePath.parent_path();
			std::filesystem::path fullTexturePath = modelDirectory / texturePath.C_Str();

			// Create and load the texture
			auto pNewTexture = std::make_unique<Texture>();

			pNewTexture->InitFromFile(device, commandPool, fullTexturePath, VK_FORMAT_R8G8B8A8_UNORM);
			m_DeletionQueue.PushDeleter(pNewTexture->GetDeletionFlush());

			pNewMaterial->metalicRoughnessTextureIndex = static_cast<uint32_t>(m_Textures.size());
			m_Textures.push_back(std::move(pNewTexture));
		}
		else // Fallback
		{
#ifdef DEBUG
			std::cout << "Material " << i << " has no metalness-roughness texture. Using default texture." << std::endl;
#endif // DEBUG
			pNewMaterial->metalicRoughnessTextureIndex = DEFAULT_METALIC_ROUGHNESS_TEXTURE_INDEX;
		}

		m_Materials.push_back(std::move(pNewMaterial));
	}
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
		int materialIndex = obj.pMesh->GetPrimitives()[0].materialIndex;
		// Assign the correct material to the RenderObject
		obj.pMaterial = m_Materials[materialIndex].get();
		obj.transform = worldTransform;

		//Update the scene's AABB with the new mesh's vertices
		for (const auto& vertex : obj.pMesh->GetVertices())
		{
			glm::vec3 worldPos = glm::vec3(worldTransform * glm::vec4(vertex.position, 1.f));
			m_SceneAABB.Expand(worldPos);
		}

		m_RenderObjects.push_back(obj);
	}

	// recursive call for all children
	for (unsigned int i = 0; i < aiNodePtr->mNumChildren; i++)
	{
		ProcessNode(aiNodePtr->mChildren[i], aiScenePtr, worldTransform);
	}
}

dae::Mesh* dae::Scene::ProcessMesh(aiMesh* aiMeshPtr, const aiMatrix4x4& rootTransform, const Device& device, VkCommandPool commandPool, VkQueue graphicsQueue)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.reserve(aiMeshPtr->mNumVertices);

	// get vertices
	for (unsigned int i = 0; i < aiMeshPtr->mNumVertices; i++)
	{
		Vertex vertex{};

		// Position
		aiVector3D pos = rootTransform * aiMeshPtr->mVertices[i];
		pos *= UNIT_SCALE; // Scale the position to match the desired unit scale
		vertex.position = { pos.x, pos.y, pos.z };

		// Normal
		if (aiMeshPtr->HasNormals())
		{
			aiMatrix3x3 normalMatrix = aiMatrix3x3(rootTransform);
			normalMatrix = normalMatrix.Inverse().Transpose();

			aiVector3D normal = normalMatrix * aiMeshPtr->mNormals[i];
			normal.Normalize();

			vertex.normal = { normal.x, normal.y, normal.z };
		}
		else
		{
			vertex.normal = { 0.0f, 1.0f, 0.0f }; // Default
		}

		if (aiMeshPtr->HasTangentsAndBitangents())
		{
			aiMatrix3x3 normalMatrix = aiMatrix3x3(rootTransform);
			normalMatrix = normalMatrix.Inverse().Transpose();

			aiVector3D tangent = normalMatrix * aiMeshPtr->mTangents[i];
			aiVector3D bitangent = normalMatrix * aiMeshPtr->mBitangents[i];

			tangent.Normalize();
			bitangent.Normalize();

			vertex.tangent = { tangent.x, tangent.y, tangent.z, 1.0f };

			// Set the handedness in w
			glm::vec3 N = vertex.normal;
			glm::vec3 T = glm::vec3(vertex.tangent);
			glm::vec3 B = {
				aiMeshPtr->mBitangents[i].x,
				aiMeshPtr->mBitangents[i].y,
				aiMeshPtr->mBitangents[i].z
			};

			vertex.tangent.w =
				glm::dot(glm::cross(N, T), B) < 0.0f
				? -1.0f
				: 1.0f;
		}
		else
			vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.f }; // Default tangent pointing right

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

void dae::Scene::LoadMaterials(const Renderer& renderer)
{
	const Device& device = renderer.GetDevice();
	VkDescriptorSetLayout textureLayout = renderer.GetGBufferPipeline().GetDescriptorSetLayout();

	// Create a descriptor set for the textures of this scene
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = MAX_MATERIALS;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	vkCreateDescriptorPool(device.GetLogicalDevice(), &poolInfo, nullptr, &m_TextureDescriptorPool);
	m_DeletionQueue.PushDeleter([pool = m_TextureDescriptorPool, logDev = renderer.GetDevice().GetLogicalDevice()]() {
		vkDestroyDescriptorPool(logDev, pool, nullptr);
		});

	// Allocate the Variable Count Set
	uint32_t texturecount[] = { static_cast<uint32_t>(m_Textures.size()) };

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableAllocInfo{};
	variableAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableAllocInfo.descriptorSetCount = 1;
	variableAllocInfo.pDescriptorCounts = texturecount;

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = &variableAllocInfo;
	allocInfo.descriptorPool = m_TextureDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &textureLayout;

	vkAllocateDescriptorSets(device.GetLogicalDevice(), &allocInfo, &m_TextureDescriptorSet);

	// Write all textures into the array
	std::vector<VkDescriptorImageInfo> imageInfos{};
	imageInfos.reserve(m_Textures.size());
	for (const auto& tex : m_Textures) {
		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = tex->GetImage().GetImageView();
		info.sampler = tex->GetSampler();
		imageInfos.push_back(info);
	}

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = m_TextureDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0; // Start writing at index 0
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = static_cast<uint32_t>(imageInfos.size());
	descriptorWrite.pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(device.GetLogicalDevice(), 1, &descriptorWrite, 0, nullptr);
}

void dae::Scene::LoadLights(const Renderer& renderer)
{
	const Device& device = renderer.GetDevice();
	VkCommandPool commandPool = renderer.GetCommandPool();
	VkDescriptorSetLayout shadowLayout = renderer.GetShadowDescriptorSetLayout();

	//load (and calculate) the light depth maps into the gpu
	std::vector<VkImageView> shadowViews{};
	for (auto& light : m_Lights) {
		light->Init(renderer, *this); // allocates depth image
		m_DeletionQueue.PushDeleter(light->GetDeletionFlush());
		if(light->GetDepthView() != VK_NULL_HANDLE) //if there is a depth view
		{
			light->SetDepthIndex(static_cast<uint32_t>(shadowViews.size()));
			shadowViews.push_back(light->GetDepthView());
		}
	}

	// Pool
	VkDescriptorPoolSize shadowPoolSize{};
	shadowPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowPoolSize.descriptorCount = MAX_LIGHTS;

	VkDescriptorPoolCreateInfo shadowPoolInfo{};
	shadowPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	shadowPoolInfo.poolSizeCount = 1;
	shadowPoolInfo.pPoolSizes = &shadowPoolSize;
	shadowPoolInfo.maxSets = 1;

	vkCreateDescriptorPool(device.GetLogicalDevice(), &shadowPoolInfo, nullptr, &m_ShadowDescriptorPool);
	m_DeletionQueue.PushDeleter([pool = m_ShadowDescriptorPool, dev = device.GetLogicalDevice()]() {
		vkDestroyDescriptorPool(dev, pool, nullptr);
		});

	// Variable count alloc
	uint32_t shadowCount[] = { static_cast<uint32_t>(shadowViews.size()) };

	VkDescriptorSetVariableDescriptorCountAllocateInfo shadowVariableInfo{};
	shadowVariableInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	shadowVariableInfo.descriptorSetCount = 1;
	shadowVariableInfo.pDescriptorCounts = shadowCount;

	VkDescriptorSetAllocateInfo shadowAllocInfo{};
	shadowAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	shadowAllocInfo.pNext = &shadowVariableInfo;
	shadowAllocInfo.descriptorPool = m_ShadowDescriptorPool;
	shadowAllocInfo.descriptorSetCount = 1;
	shadowAllocInfo.pSetLayouts = &shadowLayout;

	vkAllocateDescriptorSets(device.GetLogicalDevice(), &shadowAllocInfo, &m_ShadowDescriptorSet);

	// Bulk write
	std::vector<VkDescriptorImageInfo> shadowImageInfos{};
	for (VkImageView view : shadowViews) {
		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = view;
		info.sampler = renderer.GetShadowSampler(); // compare sampler from renderer
		shadowImageInfos.push_back(info);
	}

	VkWriteDescriptorSet shadowWrite{};
	shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowWrite.dstSet = m_ShadowDescriptorSet;
	shadowWrite.dstBinding = 0;
	shadowWrite.dstArrayElement = 0;
	shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowWrite.descriptorCount = static_cast<uint32_t>(shadowImageInfos.size());
	shadowWrite.pImageInfo = shadowImageInfos.data();

	vkUpdateDescriptorSets(device.GetLogicalDevice(), 1, &shadowWrite, 0, nullptr);
}

void dae::Scene::CreateDefaultTextures(const Device& device, VkCommandPool commandPool)
{
	uint32_t whitePixel[4] =
	{
		255, 255, 255, 255
	};

	uint64_t normalPixel[4] =
	{
		128, 128, 255, 255
	};

	uint32_t metallicRoughnessPixel[4] =
	{
		0, 255, 0, 0
	};

	//create a backup white texture
	m_Textures.push_back(std::make_unique<Texture>()); // index 0 will be the default white texture
	Texture* pDefaultTexture = m_Textures.back().get();

	pDefaultTexture->Init(device, 1, 1, VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1);

	pDefaultTexture->LoadPixels(device, commandPool, whitePixel, 1, 1);
	m_DeletionQueue.PushDeleter(pDefaultTexture->GetDeletionFlush());

	//create a backup normal texture
	m_Textures.push_back(std::make_unique<Texture>()); // index 1 will be the default normal texture
	Texture* pDefaultNormalTexture = m_Textures.back().get();

	pDefaultNormalTexture->Init(device, 1, 1, VK_FORMAT_R16G16B16A16_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1);

	pDefaultNormalTexture->LoadPixels(device, commandPool, normalPixel, 1, 1, 8u);
	m_DeletionQueue.PushDeleter(pDefaultNormalTexture->GetDeletionFlush());

	//create a backup metallic-roughness texture
	m_Textures.push_back(std::make_unique<Texture>()); // index 2 will be the default metallic-roughness texture
	Texture* pDefaultMetallicRoughnessTexture = m_Textures.back().get();
	pDefaultMetallicRoughnessTexture->Init(device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1);

	pDefaultMetallicRoughnessTexture->LoadPixels(device, commandPool, metallicRoughnessPixel, 1, 1);
	m_DeletionQueue.PushDeleter(pDefaultMetallicRoughnessTexture->GetDeletionFlush());
}
