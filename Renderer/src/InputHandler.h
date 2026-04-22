#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#include "pch.h"

namespace dae
{
	class Window;

	class InputHandler
	{
	public:
		InputHandler(Window* window);
		~InputHandler() = default;

		void Update();

		// Keyboard
		bool IsKeyPressed(int key) const;   // Just pressed this frame
		bool IsKeyReleased(int key) const;  // Just released this frame
		bool IsKeyDown(int key) const;      // Currently held down

		// Mouse
		bool IsMouseButtonPressed(int button) const;
		bool IsMouseButtonReleased(int button) const;
		bool IsMouseButtonDown(int button) const;
		int GetMouseX() const;
		int GetMouseY() const;
		glm::vec2 GetMousePos() const;
		glm::vec2 GetMouseDelta() const;

	private:
		Window* m_Window;

		// Keyboard state
		static constexpr int MAX_KEYS = GLFW_KEY_LAST + 1;
		std::array<bool, MAX_KEYS> m_CurrentKeys{};
		std::array<bool, MAX_KEYS> m_PreviousKeys{};
		std::array<bool, MAX_KEYS> m_ChangedKeys{};
		std::array<bool, MAX_KEYS> m_PressedKeys{};
		std::array<bool, MAX_KEYS> m_ReleasedKeys{};

		// Mouse state
		static constexpr int MAX_BUTTONS = GLFW_MOUSE_BUTTON_LAST + 1;
		std::array<bool, MAX_BUTTONS> m_CurrentMouseButtons{};
		std::array<bool, MAX_BUTTONS> m_PreviousMouseButtons{};
		std::array<bool, MAX_BUTTONS> m_ChangedMouseButtons{};
		std::array<bool, MAX_BUTTONS> m_PressedMouseButtons{};
		std::array<bool, MAX_BUTTONS> m_ReleasedMouseButtons{};

		int m_MouseX = 0;
		int m_MouseY = 0;
		int m_PreviousMouseX = 0;
		int m_PreviousMouseY = 0;
	};
}

#endif // !INPUTHANDLER_H