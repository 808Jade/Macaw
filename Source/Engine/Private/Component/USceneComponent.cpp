/**
 * @file USceneComponent.cpp
 * @brief 검증된 위치, 회전과 스케일을 보관하는 공간 컴포넌트입니다.
 * @ingroup Engine
 */

#include "Engine/Component/USceneComponent.h"
#include "Engine/AActor.h"
#include "Engine/Collision/FHitResult.h"
#include "Core/Reflection/EPropertyFlags.h"
#include "Core/Math/FMatrix.h"
#include "Core/Math/FTransform.h"

namespace Engine {

    /**
     * @brief USceneComponent의 초기 상태와 전달받은 의존성을 구성합니다.
     */
    Engine::USceneComponent::USceneComponent() = default;

    /**
     * @brief 객체의 수명을 끝내고 소유한 리소스를 정리합니다.
     */
    Engine::USceneComponent::~USceneComponent() = default;

    /**
     * @brief 컴포넌트의 위치·회전·스케일을 반환합니다.
     */
    const FTransform& Engine::USceneComponent::GetTransform() const {
        return mTransform;
    }

    /**
     * @brief 유한하고 유효한 변환을 검증하여 공간 컴포넌트에 적용합니다.
     */
    bool Engine::USceneComponent::SetTransform(const FTransform& Transform) {
        if (!Transform.IsValid()) {
            return false;
        }

        mTransform = Transform;

        return true;
    }

    /**
     * @brief 위치, 회전과 스케일이 적용된 월드 행렬을 반환합니다.
     */
    FMatrix Engine::USceneComponent::GetWorldMatrix() const {
        return mTransform.ToMatrix();
    }

    /**
     * @brief 컴포넌트를 월드 변위만큼 옮기고, 요청하면 이동 경로를 쓸어 막히는 지점에서 멈춥니다.
     */
    bool Engine::USceneComponent::MoveComponent(const FVector& Delta, bool Sweep, Engine::FHitResult* OutHit) {
        Engine::FHitResult Hit{};
        const FVector Start{mTransform.mLocation};
        Hit.Init(Start, Start + Delta);

        bool Moved{};

        if (Delta.IsFinite()) {
            Moved = Delta.IsNearlyZero() || MoveComponentImpl(Delta, Sweep, Hit);
        }

        if (OutHit != nullptr) {
            *OutHit = Hit;
        }

        return Moved;
    }

    /**
     * @brief MoveComponent의 실제 이동을 처리합니다. 기본 구현은 쓸지 않고 Delta만큼 옮깁니다.
     */
    bool Engine::USceneComponent::MoveComponentImpl(const FVector& Delta, bool, Engine::FHitResult&) {
        return ApplyMove(Delta);
    }

    /**
     * @brief 위치만 Delta만큼 옮깁니다. 소유 액터의 루트이면 액터 전체를 옮깁니다.
     */
    bool Engine::USceneComponent::ApplyMove(const FVector& Delta) {
        FTransform Transform{mTransform};
        Transform.mLocation += Delta;

        // 컴포넌트 계층이 없으므로 루트를 옮길 때는 액터가 나머지 컴포넌트를 함께 옮기게 합니다.
        if (Engine::AActor* Owner{GetOwner()}; Owner != nullptr && Owner->GetRootComponent() == this) {
            return Owner->SetActorTransform(Transform);
        }

        return SetTransform(Transform);
    }

}
