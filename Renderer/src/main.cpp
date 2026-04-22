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

	return 0;
}