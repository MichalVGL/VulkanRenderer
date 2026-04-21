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

		void SetResizeCallback(GLFWframebuffersizefun callback);

		GLFWwindow* GetGLFWwindow() const { return m_pWindow.get(); }
		glm::ivec2 GetSize() const { return m_Size; }
		const std::string& GetTitle() const { return m_Title; }

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

		glm::ivec2 m_Size{};
		std::string m_Title{};

	};
}

#endif // !WINDOW_H
