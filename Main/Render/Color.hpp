#pragma once

#include "glm/glm/glm.hpp"
#include <algorithm>

/*
    reference : https://github.com/ixchow/15-466-ibl/blob/master/rgbe.hpp
*/

inline glm::vec3 RGBE2Float(glm::u8vec4 InRGBE)
{
    // avoid decoding zero to a denormalized value:
    if(InRGBE == glm::u8vec4(0,0,0,0))
    {
        return glm::vec3(0,0,0);
    }

    int Exp = int(InRGBE.a) - 128;
    return glm::vec3(
		std::ldexp((InRGBE.r + 0.5f) / 256.0f, Exp),
		std::ldexp((InRGBE.g + 0.5f) / 256.0f, Exp),
		std::ldexp((InRGBE.b + 0.5f) / 256.0f, Exp)
	);
}

inline glm::u8vec4 Float2RGBE(glm::vec3 InVec3)
{
    float D = std::max(InVec3.r, std::max(InVec3.g, InVec3.b));

	// 1e-32 is from the radiance code, and is probably larger than strictly necessary:
	if (D <= 1e-32f) 
    {
		return glm::u8vec4(0,0,0,0);
	}

	int E;
	float Fac = 255.999f * (std::frexp(D, &E) / D);

	// value is too large to represent, clamp to bright white:
	if (E > 127) {
		return glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	}

	// scale and store:
	return glm::u8vec4(
		std::max(0, int32_t(InVec3.r * Fac)),
		std::max(0, int32_t(InVec3.g * Fac)),
		std::max(0, int32_t(InVec3.b * Fac)),
		E + 128
	);
}