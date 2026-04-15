#include "Renderer.h"

void dae::Renderer::Init()
{
	if (m_IsInitialized)
	{
		DeleteResources();
	}

	//todo, add initialization code here
	//	extension, vulkan, ...0

	m_IsInitialized = true;

}

void dae::Renderer::DeleteResources()
{
	m_DeletionQueue.Flush();
}
