#ifndef RENDERER_H
#define RENDERER_H

#include "pch.h"

#include "DeletionQueue.h"

namespace dae
{
	//class RendererBuilder final
	//{
	//public:
	//
	//
	//
	//private:
	//
	//
	//}

	class Renderer final
	{
	public:

		Renderer() = default;

		void Init();

	private:

		void DeleteResources();

		bool m_IsInitialized{ false };

		GLFWwindow* m_pWindow;

		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;

		DeletionQueue m_DeletionQueue{};
	};
}

#endif // !RENDERER_H
