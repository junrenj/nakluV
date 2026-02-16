#pragma once
#include "Animation.hpp"

class UAnimPlayer
{
public:
    static void UpdateAnimations(const float DeltaTime);
    static void ResetAnimationsTime();
    static inline std::vector<UAnimInstance*> AnimInstances;
private:
    static inline float AnimationTime = 0.0f;
    static void UpdateAnimation(const UAnimInstance& AnimInstance, const float CurrentTime);
};