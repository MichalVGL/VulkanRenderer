#include "pch.h"
#include "Renderer.h"
#include "Window.h"
#include "InputHandler.h"

int main()
{
	
	// Window
	dae::Window window{};
	window.Init({ 800, 600 }, "Vulkan Renderer");

	// Input Handler
	dae::InputHandler inputHandler{ &window };

	// Renderer
	dae::Renderer renderer{};
	renderer.Init(&window);

	// Main loop
	while (!window.ShouldClose())
	{
		inputHandler.Update();

		renderer.DrawFrame();

		glfwPollEvents();
	}
	

	//Assimp::Importer importer;
	//
	//const aiScene* scene = importer.ReadFile(
	//	"resources/gltf/Sponza/Sponza.gltf",
	//	aiProcess_Triangulate |
	//	aiProcess_JoinIdenticalVertices |
	//	aiProcess_GenNormals |
	//	aiProcess_CalcTangentSpace
	//);
	//
	//if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
	//	std::cout << "Error loading model: "
	//		<< importer.GetErrorString() << std::endl;
	//	return -1;
	//}
	//
	//std::cout << "Successfully loaded glTF!" << std::endl;
	//std::cout << "Meshes: " << scene->mNumMeshes << std::endl;

	return 0;
}