#ifndef LIGHTUTILS_H
#define LIGHTUTILS_H

#include "pch.h"
#include "Light.h"

namespace dae
{
    inline glm::vec3 KelvinToRgb(float kelvin)
    {
        const float temp = kelvin / 100.0f;

        // Red
        const float r = (temp <= 66.0f)
            ? 1.0f
            : glm::clamp(1.292935618906f * std::pow(temp - 60.0f, -0.1332047592f), 0.0f, 1.0f);

        // Green
        const float g = (temp <= 66.0f)
            ? glm::clamp(0.390081578769f * std::log(temp) - 0.63141844378f, 0.0f, 1.0f)
            : glm::clamp(1.12989086089f * std::pow(temp - 60.0f, -0.0755148492f), 0.0f, 1.0f);

        // Blue
        const float b = (temp >= 66.0f)
            ? 1.0f
            : (temp <= 19.0f)
            ? 0.0f
            : glm::clamp(0.54320678911f * std::log(temp - 10.0f) - 1.19625408914f, 0.0f, 1.0f);

        return glm::vec3(r, g, b);
    }

    //todo, delete
    /*

    inline GPULight ConvertPointLight(const CPUPointLight& light)
    {
        GPULight gpu{};
        gpu.posDir = glm::vec4(light.pos, 1.0f);       // w > 0 = point light
        gpu.color = glm::vec4(KelvinToRgb(light.temperature), light.lumen);
        return gpu;
    }

    inline GPULight ConvertDirectionalLight(const CPUDirectionalLight& light)
    {
        GPULight gpu{};
        gpu.posDir = glm::vec4(glm::normalize(light.dir), -1.0f); // w < 0 = directional
        gpu.color = glm::vec4(KelvinToRgb(light.temperature), light.lux);
        return gpu;
    }
    //*/
}

#endif