#ifndef MATHHELPERS_H
#define MATHHELPERS_H

#include "pch.h"

namespace dae
{
	constexpr inline bool IsZero(std::floating_point auto v)
	{
		return std::abs(v) < FLT_EPSILON;
	}

	constexpr inline bool IsZero(glm::vec3 v)
	{
		return IsZero(v.x) && IsZero(v.y) && IsZero(v.z);
	}

	constexpr glm::vec3 g_Up{ 0.f, 1.f, 0.f };
	constexpr glm::vec3 g_Right{ 1.f, 0.f, 0.f };
	constexpr glm::vec3 g_Forward{ 0.f, 0.f, 1.f };
	constexpr glm::vec3 g_Down{ 0.f, -1.f, 0.f };
	constexpr glm::vec3 g_Left{ -1.f, 0.f, 0.f };
	constexpr glm::vec3 g_Backward{ 0.f, 0.f, -1.f };
}

#endif // !MATHHELPERS_H