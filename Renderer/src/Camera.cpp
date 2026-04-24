#include "Camera.h"

#include <cassert>

#include "InputHandler.h"
#include "MathHelpers.h"

dae::Camera::Camera()
{
	CalculateFOVAspect();
}

void dae::Camera::Update(float deltaTime, const dae::InputHandler& input)
{

	//speed	(only for wasd)
	float speed = m_BaseSpeed;
	if (input.IsKeyDown(GLFW_KEY_LEFT_SHIFT))
		speed *= 2.f;

	/* WASD input
	It calculates the input into a Vector3 with the ranges : [-1, 1]; 0; [-1, 1]
	This represents the keys in world space after which it will be rotated with the pitch and yaw
	and then applied to the position of the camera.
	*/
	glm::vec3 keyDirection{ static_cast<float>(input.IsKeyDown(GLFW_KEY_D) + (-static_cast<int>(input.IsKeyDown(GLFW_KEY_A))))	//X
	, 0.f																																//Y
	, static_cast<float>(input.IsKeyDown(GLFW_KEY_W) + (-static_cast<int>(input.IsKeyDown(GLFW_KEY_S)))) };						//Z

	if (!dae::IsZero(keyDirection))
	{
		keyDirection = glm::normalize(keyDirection);

		glm::vec3 worldDir =
			keyDirection.x * m_Right +
			keyDirection.y * m_Up +
			keyDirection.z * -m_Forward;

		SetOrigin(m_Origin += (worldDir * deltaTime * speed));
	}

	//Mouse Input
	if (input.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT) && input.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
	{
		//Get the Y movement of the mouse and add to the world height of the camera (unitY)
		glm::vec3 moveDirection{ g_Up * static_cast<float>(input.GetMouseDelta().y) * m_MouseSpeed };
		SetOrigin(m_Origin += (moveDirection));
	}
	else if (input.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
	{
		//Set yaw and pitch according to mouse Y/X
		SetPitch(m_Pitch += glm::radians(static_cast<float>(-input.GetMouseDelta().y) * m_RotationSensitivity));
		SetYaw(m_Yaw += glm::radians(static_cast<float>(input.GetMouseDelta().x) * m_RotationSensitivity));
	}
	else if (input.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
	{
		//Get the Y movement of the mouse and add to the forward vector of the camera
		SetYaw(m_Yaw += glm::radians(static_cast<float>(input.GetMouseDelta().x) * m_RotationSensitivity));

		glm::vec3 mouseDirection{ g_Forward * static_cast<float>(-input.GetMouseDelta().y) * m_MouseSpeed };	// camera space vector

		glm::vec3 worldDir =
			mouseDirection.x * m_Right +
			mouseDirection.y * m_Up +
			mouseDirection.z * m_Forward;

		// create the vector in worldspace, then apply the direction to the camera (world)position
		SetOrigin(m_Origin += worldDir);
	}
}

void dae::Camera::SetOrigin(const glm::vec3& origin)
{
	m_Origin = origin;
	CalculateCameraToWorld();
}

glm::vec3 dae::Camera::GetOrigin() const
{
	return m_Origin;
}

void dae::Camera::SetFOV(float angle)
{
	m_FovAngle = angle;
	CalculateFOVAspect();
}

float dae::Camera::GetFOV() const
{
	return m_FovAngle;
}

float dae::Camera::GetFOVAspect() const
{
	return m_FOVAspect;
}

void dae::Camera::SetPitch(float pitch)
{
	m_Pitch = std::clamp(pitch, -s_MaxPitch, s_MaxPitch);

	CalculateDirections();
	CalculateCameraToWorld();
}

void dae::Camera::SetYaw(float yaw)
{
	m_Yaw = yaw;
	CalculateDirections();
	CalculateCameraToWorld();
}

glm::mat4x4 dae::Camera::GetCameraToWorld() const
{
	return glm::inverse(m_ONB);
}

void dae::Camera::CalculateFOVAspect()
{
	m_FOVAspect = tan((glm::radians(m_FovAngle)) / 2.f);
}

void dae::Camera::CalculateCameraToWorld()
{
	m_ONB[0] = glm::vec4(m_Right, 0.0f);
	m_ONB[1] = glm::vec4(m_Up, 0.0f);
	m_ONB[2] = glm::vec4(m_Forward, 0.0f);
	m_ONB[3] = glm::vec4(m_Origin, 1.0f);
}

void dae::Camera::CalculateDirections()
{
	m_Forward.x = cosf(m_Pitch) * sinf(m_Yaw);
	m_Forward.y = sinf(m_Pitch);
	m_Forward.z = -cosf(m_Pitch) * cosf(m_Yaw);
	m_Forward = glm::normalize(m_Forward);

	m_Right = glm::normalize(glm::cross(g_Up, m_Forward));
	m_Up = glm::normalize(glm::cross(m_Forward, m_Right));
}
