#ifndef LIGHT_H
#define LIGHT_H

#include "LightUtils.h"
#include "Image.h"

namespace dae
{
	class Renderer;
	struct AABB;
	class Scene;

	//data for lighting pass (ssbo)
	struct GPULight {
		glm::vec4 posDir;		// xyz = position or direction, w = type flag (<0 for directional, >0 for point)
		glm::vec4 color; 		// rgb = converted color, a = lumen or lux depending on type
		glm::mat4 lightView;
		glm::mat4 lightProj;
		uint32_t depthIndex;
		uint32_t padding[3];
	};

	//push constant for shadow generation pass
	struct ShadowPushConstants 
	{
		glm::mat4 lightSpaceTransform;
		uint64_t vertexBufferAddress{};
	};

	enum class LightType
	{
		Point,
		Directional
	};

	class Light final
	{
	public:

		static std::unique_ptr<Light> CreateDirectional(glm::vec3 direction, uint32_t shadowResolution) {
			auto light = std::unique_ptr<Light>(new Light(direction, LightType::Directional, shadowResolution));
			return light;
		}

		static std::unique_ptr<Light> CreatePoint(glm::vec3 position, uint32_t shadowResolution) {
			auto light = std::unique_ptr<Light>(new Light(position, LightType::Point, shadowResolution));
			return light;
		}

		~Light() = default;

		Light(const Light&) = delete;
		Light& operator=(const Light&) = delete;
		Light(Light&&) = delete;
		Light& operator=(Light&&) = delete;

		void Init(const Renderer& renderer, const Scene& scene);

		void SetLumenOrLux(float value) { m_LumenOrLux = value; }
		void SetTemperature(float kelvin) { temperature = kelvin; }
		void SetDepthIndex(uint32_t index) { m_DepthIndex = index; }

		GPULight GetGPULightData() const;
		VkImageView GetDepthView() const { return m_DepthImage.GetImageView(); }

		std::function<void()> GetDeletionFlush();

	private:

		Light(glm::vec3 posOrDir, LightType type, uint32_t shadowResolution)
			: m_PosOrDir(posOrDir), m_Type(type), m_DepthResolution(shadowResolution) {
		}

		/*
			if shadow mapping for point light were to be implemented,
				return 6 image views of every face so you can use the same array of shadowsamplers as dir lights
				just make sure to assign "6" indices to the point light and compensate with it in LoadLights() in scene.cpp
		*/

		//todo, use std::variant for the cubemap if point light supports shadowmapping
		Image m_DepthImage{};
		uint32_t m_DepthIndex{ 0u }; // Index in the depth image array for shadow mapping

		float m_LumenOrLux{}; // Lumen for point lights, Lux for directional lights
		float temperature{}; // Color temperature in Kelvin

		const glm::vec3 m_PosOrDir{};
		const uint32_t m_DepthResolution{};
		const LightType m_Type{};

		glm::mat4 m_View{};
		glm::mat4 m_Proj{};

		DeletionQueue m_DeletionQueue{};

		//values passed to the gpu in their W component, this allows the shaders to determine the light type
		static constexpr float POINT_W_VALUE = 1.0f;
		static constexpr float DIRECTIONAL_W_VALUE = -1.0f;
	};
}

#endif // !LIGHT_H
