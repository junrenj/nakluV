#pragma once
#include <vector>

class UNode;

class UAnimInstance
{
public:
    UNode* Node;
    enum class EChannel 
    {
        Translation,
        Scale,
        Rotation
    } Channel;

    std::vector< float > Times;
    std::vector< float > Values;

    enum class EInterpolation
    {
        STEP,
		LINEAR,
		SLERP,
    }Interpolation = EInterpolation::LINEAR;
};