#include "Window.h"

dae::Window::Window()
{
	glfwInit();
}

dae::Window::~Window()
{
	if (m_pWindow)
	{
		m_pWindow.reset();
	}
	glfwTerminate();
}

void dae::Window::Init(glm::ivec2 size, const std::string& title)
{
	if (m_pWindow)
	{
		m_pWindow.reset();
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_pWindow = GLFWwindowPtr(glfwCreateWindow(size.x, size.y, title.c_str(), nullptr, nullptr));
	glfwSetWindowUserPointer(m_pWindow.get(), this);
	glfwSetFramebufferSizeCallback(m_pWindow.get(), framebufferResizeCallback);

	m_Size = size;
	m_Title = title;
}

bool dae::Window::IsInitialized() const
{
	return m_pWindow != nullptr;
}

bool dae::Window::ShouldClose() const
{
	return glfwWindowShouldClose(m_pWindow.get());
}