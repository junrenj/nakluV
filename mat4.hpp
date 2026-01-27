#pragma once

// A *small* matrix math library for 4x4 matrices only.

#include <array>
#include <cmath>
#include <cstdint>

// NOTE: column-major storage order (like in OpenGL / GLSL):
using Mat4 = std::array< float, 16 >;
static_assert(sizeof(Mat4) == 16*4, "Mat4 is exactly 16 32-bit floats.");

using Vec4 = std::array< float, 4 >;
static_assert(sizeof(Vec4) == 4*4, "Vec4 is exactly 4 32-bit floats.");

inline Vec4 operator*(Mat4 const &A, Vec4 const &b) 
{
	Vec4 ret;
	// compute ret = A * b:
	for (uint32_t r = 0; r < 4; ++r) 
    {
		ret[r] = A[0 * 4 + r] * b[0];
		for (uint32_t k = 1; k < 4; ++k) 
        {
			ret[r] += A[k * 4 + r] * b[k];
		}
	}
	return ret;
}

inline Mat4 operator*(Mat4 const &A, Mat4 const &B) 
{
	Mat4 ret;
	// compute ret = A * B:
	for (uint32_t c = 0; c < 4; ++c) 
    {
		for (uint32_t r = 0; r < 4; ++r) 
        {
			ret[c * 4 + r] = A[0 * 4 + r] * B[c * 4 + 0];
			for (uint32_t k = 1; k < 4; ++k) 
            {
				ret[c * 4 + r] += A[k * 4 + r] * B[c * 4 + k];
			}
		}
	}
	return ret;
}

// perspective projection matrix.
// - vfov is fov *in radians*
// - near maps to 0, far maps to 1
// looks down -z with +y up and +x right
inline Mat4 Perspective(float vfov, float aspect, float near, float far) 
{
	// as per https://www.terathon.com/gdc07_lengyel.pdf
	// (with modifications for Vulkan-style coordinate system)
	//  notably: flip y (vulkan device coords are y-down)
	//       and rescale z (vulkan device coords are z-[0,1])
	const float e = 1.0f / std::tan(vfov / 2.0f);
	const float a = aspect;
	const float n = near;
	const float f = far;
	return Mat4
    {   //note: column-major storage order!
		e/a,  0.0f,                      0.0f, 0.0f,
		0.0f,   -e,                      0.0f, 0.0f,
		0.0f, 0.0f,-0.5f - 0.5f * (f+n)/(f-n),-1.0f,
		0.0f, 0.0f,             - (f*n)/(f-n), 0.0f,
	};
}

// look at matrix:
// makes a camera-space-from-world matrix for a camera at eye looking toward
// target with up-vector pointing (as-close-as-possible) along up.
// That is, it maps:
//  - eye_xyz to the origin
//  - the unit length vector from eye_xyz to target_xyz to -z
//  - an as-close-as-possible unit-length vector to up to +y
inline Mat4 Look_at
(
	float eye_x, float eye_y, float eye_z,
	float target_x, float target_y, float target_z,
	float up_x, float up_y, float up_z ) {

	// NOTE: this would be a lot cleaner with a vec3 type and some overloads!

	// compute vector from eye to target:
	float in_x = target_x - eye_x;
	float in_y = target_y - eye_y;
	float in_z = target_z - eye_z;

	// normalize 'in' vector:
	float inv_in_len = 1.0f / std::sqrt(in_x*in_x + in_y*in_y + in_z*in_z);
	in_x *= inv_in_len;
	in_y *= inv_in_len;
	in_z *= inv_in_len;

	// make 'up' orthogonal to 'in':
	float in_dot_up = in_x*up_x + in_y*up_y +in_z*up_z;
	up_x -= in_dot_up * in_x;
	up_y -= in_dot_up * in_y;
	up_z -= in_dot_up * in_z;

	// normalize 'up' vector:
	float inv_up_len = 1.0f / std::sqrt(up_x*up_x + up_y*up_y + up_z*up_z);
	up_x *= inv_up_len;
	up_y *= inv_up_len;
	up_z *= inv_up_len;

	// compute 'right' vector as 'in' x 'up'
	float right_x = in_y*up_z - in_z*up_y;
	float right_y = in_z*up_x - in_x*up_z;
	float right_z = in_x*up_y - in_y*up_x;

	// compute dot products of right, in, up with eye:
	float right_dot_eye = right_x*eye_x + right_y*eye_y + right_z*eye_z;
	float up_dot_eye = up_x*eye_x + up_y*eye_y + up_z*eye_z;
	float in_dot_eye = in_x*eye_x + in_y*eye_y + in_z*eye_z;

	//final matrix: (computes (right . (v - eye), up . (v - eye), -in . (v-eye), v.w )
	return Mat4
    {   //note: column-major storage order
		right_x, up_x, -in_x, 0.0f,
		right_y, up_y, -in_y, 0.0f,
		right_z, up_z, -in_z, 0.0f,
		-right_dot_eye, -up_dot_eye, in_dot_eye, 1.0f,
	};
}

// Orbit camera matrix:
// makes a camera-from-world matrix for a camera orbiting target_{x,y,z}
//   at distance radius with angles azimuth and elevation.
// azimuth is counterclockwise angle in the xy plane from the x axis
// elevation is angle up from the xy plane
// both are in radians
inline Mat4 orbit(
		float target_x, float target_y, float target_z,
		float azimuth, float elevation, float radius
	) {

	// shorthand for some useful trig values:
	float CA = std::cos(azimuth);
	float SA = std::sin(azimuth);
	float CE = std::cos(elevation);
	float SE = std::sin(elevation);

	// Camera's right direction is azimuth rotated by 90 degrees:
	float Right_X = -SA;
	float Right_Y = CA;
	float Right_Z = 0.0f;

	// camera's up direction is elevation rotated 90 degrees:
	// (and points in the same xy direction as azimuth)
	float Up_X = -SE * CA;
	float Up_Y = -SE * SA;
	float Up_Z = CE;

	// Direction to the camera from the target:
	float Out_X = CE * CA;
	float Out_Y = CE * SA;
	float Out_Z = SE;

	// Camera's position:
	float Eye_X = target_x + radius * Out_X;
	float Eye_Y = target_y + radius * Out_Y;
	float Eye_Z = target_z + radius * Out_Z;

	// Camera's position projected onto the various vectors:
	float RightDotEye = Right_X * Eye_X + Right_Y * Eye_Y + Right_Z * Eye_Z;
	float UpDotEye = Up_X * Eye_X + Up_Y * Eye_Y + Up_Z * Eye_Z;
	float OutDotEye = Out_X * Eye_X + Out_Y * Eye_Y + Out_Z * Eye_Z;

	// the final local-from-world transformation (column-major):
	return Mat4
	{
		Right_X, Up_X, Out_X, 0.0f,
		Right_Y, Up_Y, Out_Y, 0.0f,
		Right_Z, Up_Z, Out_Z, 0.0f,
		-RightDotEye, -UpDotEye, -OutDotEye, 1.0f,
	};
}