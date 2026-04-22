#include "InputHandler.h"
#include <algorithm>

#include "Window.h"

namespace dae
{
	InputHandler::InputHandler(Window* window)
		: m_Window(window)
	{
		// Initialize all keys as not pressed
		m_CurrentKeys.fill(false);
		m_PreviousKeys.fill(false);
		m_PressedKeys.fill(false);
		m_ReleasedKeys.fill(false);

		m_CurrentMouseButtons.fill(false);
		m_PreviousMouseButtons.fill(false);
		m_PressedMouseButtons.fill(false);
		m_ReleasedMouseButtons.fill(false);
	}

	void InputHandler::Update()
	{
		// === KEYBOARD ===
		// Store previous state
		m_PreviousKeys = m_CurrentKeys;

		// Update current state for all keys
		for (int key = 0; key < MAX_KEYS; ++key)
		{
			m_CurrentKeys[key] = glfwGetKey(m_Window->GetGLFWwindow(), key) == GLFW_PRESS;
		}

		// Calculate pressed and released
		std::ranges::transform(m_CurrentKeys, m_PreviousKeys, m_ChangedKeys.begin(), std::not_equal_to{});	//current != previous
		std::ranges::transform(m_ChangedKeys, m_CurrentKeys, m_PressedKeys.begin(), std::logical_and{});	//changed && current
		std::ranges::transform(m_ChangedKeys, m_CurrentKeys, m_ReleasedKeys.begin()							//changed && !current
			, [](bool changed, bool current) { return changed && !current; });


		// === MOUSE ===
		// Store previous state
		m_PreviousMouseButtons = m_CurrentMouseButtons;
		m_PreviousMouseX = m_MouseX;
		m_PreviousMouseY = m_MouseY;

		// Get current mouse position
		double mouseX, mouseY;
		glfwGetCursorPos(m_Window->GetGLFWwindow(), &mouseX, &mouseY);
		m_MouseX = static_cast<int>(mouseX);
		m_MouseY = static_cast<int>(mouseY);

		// Update mouse button states
		for (int button = 0; button < MAX_BUTTONS; ++button)
		{
			m_CurrentMouseButtons[button] = glfwGetMouseButton(m_Window->GetGLFWwindow(), button) == GLFW_PRESS;
		}

		// Calculate pressed and released

		std::ranges::transform(m_CurrentMouseButtons, m_PreviousMouseButtons, m_ChangedMouseButtons.begin(), std::not_equal_to{});	//current != previous
		std::ranges::transform(m_ChangedMouseButtons, m_CurrentMouseButtons, m_PressedMouseButtons.begin(), std::logical_and{});	//changed && current
		std::ranges::transform(m_ChangedMouseButtons, m_CurrentMouseButtons, m_ReleasedMouseButtons.begin()							//changed && !current
					, [](bool changed, bool current) { return changed && !current; });	
	}

	bool InputHandler::IsKeyPressed(int key) const
	{
		if (key < 0 || key >= MAX_KEYS) return false;
		return m_PressedKeys[key];
	}

	bool InputHandler::IsKeyReleased(int key) const
	{
		if (key < 0 || key >= MAX_KEYS) return false;
		return m_ReleasedKeys[key];
	}

	bool InputHandler::IsKeyDown(int key) const
	{
		if (key < 0 || key >= MAX_KEYS) return false;
		return m_CurrentKeys[key];
	}

	bool InputHandler::IsMouseButtonPressed(int button) const
	{
		if (button < 0 || button >= MAX_BUTTONS) return false;
		return m_PressedMouseButtons[button];
	}

	bool InputHandler::IsMouseButtonReleased(int button) const
	{
		if (button < 0 || button >= MAX_BUTTONS) return false;
		return m_ReleasedMouseButtons[button];
	}

	bool InputHandler::IsMouseButtonDown(int button) const
	{
		if (button < 0 || button >= MAX_BUTTONS) return false;
		return m_CurrentMouseButtons[button];
	}

	int InputHandler::GetMouseX() const
	{
		return m_MouseX;
	}

	int InputHandler::GetMouseY() const
	{
		return m_MouseY;
	}

	glm::vec2 InputHandler::GetMousePos() const
	{
		return glm::vec2(m_MouseX, m_MouseY);
	}

	glm::vec2 InputHandler::GetMouseDelta() const
	{
		return glm::vec2(m_MouseX - m_PreviousMouseX, m_MouseY - m_PreviousMouseY);
	}
}