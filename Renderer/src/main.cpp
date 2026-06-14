#include "pch.h"
#include "Renderer.h"
#include "Window.h"
#include "InputHandler.h"
#include "Scene.h"

#include <chrono>

int main()
{
	dae::DeletionQueue mainDeletionQueue{};
	
	// Window
	dae::Window window{};
	window.Init({ 1920, 1080 }, "Vulkan Renderer");

	// Input Handler
	dae::InputHandler inputHandler{ &window };

	// Renderer
	dae::Renderer renderer{};
	renderer.Init(&window);
	mainDeletionQueue.PushDeleter(renderer.GetDeletionFlush());

	// Scene
	dae::Scene scene{};
	scene.Init("resources/gltf/Sponza/Sponza.gltf", renderer);
	mainDeletionQueue.PushDeleter(scene.GetDeletionFlush());

	auto& camera = scene.GetCamera();
	camera.SetFOV(60.f);

	auto lastTime = std::chrono::high_resolution_clock::now();

	// Main loop
	while (!window.ShouldClose())
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		inputHandler.Update();
		scene.Update(deltaTime, inputHandler);

		renderer.DrawFrame(scene, window);

		glfwPollEvents();
	}

	mainDeletionQueue.Flush();

	return 0;
}