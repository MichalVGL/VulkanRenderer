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
	constexpr glm::vec3 g_Forward{ 0.f, 0.f, -1.f };
	constexpr glm::vec3 g_Down{ 0.f, -1.f, 0.f };
	constexpr glm::vec3 g_Left{ -1.f, 0.f, 0.f };
	constexpr glm::vec3 g_Backward{ 0.f, 0.f, 1.f };

	inline glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}
}

#endif // !MATHHELPERS_H