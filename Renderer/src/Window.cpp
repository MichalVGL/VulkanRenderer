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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); //todo, make it resizable

	m_pWindow = GLFWwindowPtr(glfwCreateWindow(size.x, size.y, title.c_str(), nullptr, nullptr));
	glfwSetWindowUserPointer(m_pWindow.get(), this);

	m_Size = size;
	m_Title = title;
}

bool dae::Window::IsInitialized() const
{
	return m_pWindow != nullptr;
}

void dae::Window::SetResizeCallback(GLFWframebuffersizefun callback)
{
	if (!m_pWindow)
	{
		std::cerr << "[Window.cpp] trying to set a resize callback, but the window is not initialized!\n";
		throw std::runtime_error("window is not initialized!");
	}

	glfwSetFramebufferSizeCallback(m_pWindow.get(), callback);
}
