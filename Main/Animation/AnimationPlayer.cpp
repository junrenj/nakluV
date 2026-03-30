#include "AnimationPlayer.hpp"
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/quaternion.hpp"
#include "glm/glm/gtc/type_ptr.hpp"
#include "../Render/RenderScene.hpp"

using quat = glm::quat;
using vec3  = glm::vec3;

void UAnimPlayer::UpdateAnimations(const float DeltaTime)
{
    AnimationTime += DeltaTime;
    if(AnimInstances.size() > 0)
    {
        for (const UAnimInstance* AnimInstance : AnimInstances)
        {
            UpdateAnimation(*AnimInstance, AnimationTime);
        }
    }
}

void UAnimPlayer::ResetAnimationsTime()
{
    AnimationTime = 0.0f;
}

void UAnimPlayer::UpdateAnimation(const UAnimInstance& AnimInstance, const float CurrentTime)
{
    if(AnimInstance.Times.empty())
    {
        return;
    }

    float MaxTime = AnimInstance.Times.back();
    float AnimTime = fmod(CurrentTime, MaxTime);
    
    UNode* Node = AnimInstance.Node;

    auto it = std::upper_bound(AnimInstance.Times.begin(), AnimInstance.Times.end(), AnimTime);
    uint32_t RightIdx = static_cast<uint32_t>(std::distance(AnimInstance.Times.begin(), it));
    uint32_t LeftIdx = (RightIdx == 0) ? 0 : RightIdx - 1;
    RightIdx = RightIdx >= AnimInstance.Times.size();
    RightIdx = LeftIdx;

    float DeltaTime = AnimInstance.Times[RightIdx] - AnimInstance.Times[LeftIdx];
    float Alpha = (DeltaTime > 0.0f) ? (AnimTime - AnimInstance.Times[LeftIdx]) / DeltaTime : 0.0f;
    
    switch (AnimInstance.Channel)
    {
        case UAnimInstance::EChannel::Rotation:
            quat Q1 = glm::make_quat(AnimInstance.Values.data() + (4 * LeftIdx));
            quat Q2 = glm::make_quat(AnimInstance.Values.data() + (4 * RightIdx));

            quat Result;
            if (AnimInstance.Interpolation == UAnimInstance::EInterpolation::SLERP) 
            {
                Result = glm::slerp(Q1, Q2, Alpha);
            } 
            else 
            {
                Result = glm::lerp(Q1, Q2, Alpha);
            }
            Node->Transform.Rotation = Result;
            break;
        case UAnimInstance::EChannel::Translation:
            vec3 V1 = glm::make_vec3(AnimInstance.Values.data() + (3 * LeftIdx));
            vec3 V2 = glm::make_vec3(AnimInstance.Values.data() + (3 * RightIdx));
            Node->Transform.Translation = glm::mix(V1, V2, Alpha);
            break;
        case UAnimInstance::EChannel::Scale:
            vec3 v1 = glm::make_vec3(AnimInstance.Values.data() + (3 * LeftIdx));
            vec3 v2 = glm::make_vec3(AnimInstance.Values.data() + (3 * RightIdx));
            Node->Transform.Scale = glm::mix(v1, v2, Alpha);
            break;
    }

    Node->SetDirty();
}
