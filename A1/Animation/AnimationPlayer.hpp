#pragma once
#include "Animation.hpp"

class UAnimPlayer
{
public:
    static void UpdateAnimation(UAnimInstance* AnimInstance, const float CurrentTime);
    static inline std::vector<UAnimInstance*> AnimInstances;
};