/**
 * @file FAnimationRuntime.cpp
 * @brief 본 포즈 블렌딩과 루트 모션 변환 계산을 제공합니다.
 * @ingroup Engine
 */

#include "Core/Types.h"
#include "Engine/Animation/FAnimationRuntime.h"

#include <algorithm>

namespace Engine {

    FTransform FAnimationRuntime::AccumulateRootMotion(const FTransform& A, const FTransform& B) {
        return {A.mLocation + A.mRotation.RotateVector(B.mLocation), (A.mRotation * B.mRotation).Normalized(), {1.0f, 1.0f, 1.0f}};
    }

    FTransform FAnimationRuntime::ApplyRootMotionToTransform(const FTransform& RootMotion, const FTransform& ComponentTransform, const FTransform& ActorTransform) {
        const FQuat Rotation{(ComponentTransform.mRotation * RootMotion.mRotation * ComponentTransform.mRotation.Inverse()).Normalized()};
        const FVector Scaled{RootMotion.mLocation.x * ComponentTransform.mScale.x, RootMotion.mLocation.y * ComponentTransform.mScale.y, RootMotion.mLocation.z * ComponentTransform.mScale.z};

        return {ComponentTransform.mLocation + ComponentTransform.mRotation.RotateVector(Scaled) + Rotation.RotateVector(ActorTransform.mLocation - ComponentTransform.mLocation), (Rotation * ActorTransform.mRotation).Normalized(), ActorTransform.mScale};
    }

    FTransform FAnimationRuntime::BlendTransforms(const FTransform& A, const FTransform& B, float WeightOfA) {
        const float Alpha{1.0f - WeightOfA};

        return {A.mLocation + (B.mLocation - A.mLocation) * Alpha, A.mRotation.Slerp(B.mRotation, Alpha), A.mScale + (B.mScale - A.mScale) * Alpha};
    }

    void FAnimationRuntime::BlendTwoPosesTogether(const Core::TArray<FTransform>& PoseA, const Core::TArray<FTransform>& PoseB, float WeightOfPoseA, Core::TArray<FTransform>& OutPose) {
        const float Weight{std::clamp(WeightOfPoseA, 0.0f, 1.0f)};
        const std::size_t Count{std::min(PoseA.size(), PoseB.size())};

        // 결과가 입력과 같은 배열일 수 있습니다. 본마다 두 입력을 읽은 뒤 쓰므로 겹쳐도 결과가 같습니다.
        if (&OutPose != &PoseA) {
            OutPose.resize(PoseA.size());

            for (Index_t Bone{static_cast<Index_t>(Count)}; Bone < PoseA.size(); ++Bone) {
                OutPose[Bone] = PoseA[Bone];
            }
        }

        for (Index_t Bone{}; Bone < Count; ++Bone) {
            OutPose[Bone] = BlendTransforms(PoseA[Bone], PoseB[Bone], Weight);
        }
    }

}
