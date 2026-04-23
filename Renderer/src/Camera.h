#ifndef CAMERA_H
#define CAMERA_H

#include "pch.h"

namespace dae
{
	class InputHandler;

	class Camera final
	{
	public:
		Camera();
		Camera(const glm::vec3& _origin, float _fovAngle) :
			m_Origin{ _origin },
			m_FovAngle{ _fovAngle }
		{
		}

		void Update(float deltaTime, const dae::InputHandler& input);

		void SetOrigin(const glm::vec3& origin);
		glm::vec3 GetOrigin() const;

		void SetFOV(float angle);	//in degrees
		float GetFOVAspect() const;	//returns tan(fov/2) saved in a member var

		void SetPitch(float pitch);	//in radians
		void SetYaw(float yaw);

		glm::mat4x4 GetCameraToWorld() const;

		//Frustrum
		float m_Near{ 1.f };
		float m_Far{ 1000.f };

	private:

		void CalculateFOVAspect();
		void CalculateCameraToWorld();

		void CalculateDirections();

		glm::vec3 m_Origin{};
		float m_FovAngle{ 90.f };
		float m_FOVAspect{};	//tan(fov/2)

		glm::mat4x4 m_ONB{};	//CameraToWorld(ONB) 

		glm::vec3 m_Forward{ glm::vec3(0.f, 0.f, 1.f) };
		glm::vec3 m_Up{ glm::vec3(0.f, 1.f, 0.f) };
		glm::vec3 m_Right{ glm::vec3(1.f, 0.f, 0.f) };
		static constexpr float s_MaxPitch{ glm::radians(89.9f) };
		float m_Pitch{};	//radians
		float m_Yaw{};

		float m_BaseSpeed{ 20.f };	//wasd
		float m_MouseSpeed{ 0.03f };
		float m_RotationSensitivity{ 0.1f };
	};
}

#endif // !CAMERA_H
