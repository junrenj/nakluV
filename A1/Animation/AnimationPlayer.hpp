#pragma once
#include "Animation.hpp"

class UAnimPlayer
{
public:
    static void UpdateAnimations(const float CurrentTime);
    static inline std::vector<UAnimInstance*> AnimInstances;
private:
    static void UpdateAnimation(const UAnimInstance& AnimInstance, const float CurrentTime);
};