#ifndef WINDOW_H
#define WINDOW_H

#include "pch.h"

#include "VulkanUtils.h"


namespace dae
{
	class Window final
	{
	public:

		Window();
		~Window();

		Window(const Window&) = delete;
		Window(Window&&) = delete;
		Window& operator=(const Window&) = delete;
		Window& operator=(Window&&) = delete;

		void Init(glm::ivec2 size, const std::string& title);

		bool IsInitialized() const;
		GLFWwindow* GetGLFWwindow() const { return m_pWindow.get(); }
		glm::ivec2 GetSize() const { return m_Size; }
		const std::string& GetTitle() const { return m_Title; }

		bool ShouldClose() const;

		void ClearFramebufferDirty() { m_FramebufferDirty = false; } //set by the renderer when it has handled the resize
		bool IsFramebufferDirty() const { return m_FramebufferDirty; }

		void SetFramebufferSize(int width, int height)
		{
			m_Size = { width, height };
			m_FramebufferDirty = true;
		}

	private:
		struct GLFWwindowDeleter
		{
			void operator()(GLFWwindow* ptr) const
			{
				glfwDestroyWindow(ptr);
			}
		};

		using GLFWwindowPtr = std::unique_ptr<GLFWwindow, GLFWwindowDeleter>;

		GLFWwindowPtr m_pWindow{};

		bool m_FramebufferDirty{ false };

		glm::ivec2 m_Size{};
		std::string m_Title{};
	};

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		Window* appWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		appWindow->SetFramebufferSize(width, height);
	}
}

#endif // !WINDOW_H
