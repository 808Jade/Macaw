/**
 * @file UMovementComponent.cpp
 * @brief 이동 대상의 수명과 월드 공간 이동을 처리합니다.
 * @ingroup Engine
 */

#include "Engine/Component/UMovementComponent.h"
#include "CoreUObject/Reflection/Cast.h"
#include "Engine/AActor.h"
#include "Engine/Collision/FHitResult.h"
#include "Engine/Component/UPrimitiveComponent.h"
#include "Engine/Component/USceneComponent.h"

#include <algorithm>
#include <cmath>

namespace Engine {

    UMovementComponent::UMovementComponent() {
        GetPrimaryComponentTick().SetTickEnabled(false);
    }

    void UMovementComponent::SetUpdatedComponent(Engine::USceneComponent* Component) {
        mUseOwnerRoot = false;
        SetExplicitUpdatedComponent(CoreUObject::MakeObjectRef(Component));
    }

    void UMovementComponent::SetExplicitUpdatedComponent(const CoreUObject::TObjectRef<Engine::USceneComponent>& Component) {
        // 로드와 복제에서 GUID에 핸들을 연결할 때도 이 프로퍼티의 setter를 거칩니다.
        mExplicitUpdatedComponent = Component;
        UpdateTickEnabledState();
    }

    Engine::USceneComponent* UMovementComponent::ResolveUpdatedComponent() const {
        const Engine::AActor* Owner{GetOwner()};

        if (mUseOwnerRoot) {
            return Owner != nullptr ? Owner->GetRootComponent() : nullptr;
        }

        return mExplicitUpdatedComponent.Get();
    }

    Engine::USceneComponent* UMovementComponent::GetUpdatedComponent() const {
        Engine::USceneComponent* Component{ResolveUpdatedComponent()};
        const Engine::AActor* Owner{GetOwner()};
        const Engine::AActor* TargetOwner{Component != nullptr ? Component->GetOwner() : nullptr};
        const Engine::UWorld* World{GetWorld()};

        if (World == nullptr || Owner == nullptr || Owner->IsPendingDestroy() || TargetOwner == nullptr || TargetOwner->IsPendingDestroy() || Component->GetWorld() != World) {
            return nullptr;
        }

        return Component;
    }

    Engine::UPrimitiveComponent* UMovementComponent::GetUpdatedPrimitive() const {
        return CoreUObject::Cast<Engine::UPrimitiveComponent>(GetUpdatedComponent());
    }

    const FVector& UMovementComponent::GetVelocity() const {
        return mVelocity;
    }

    bool UMovementComponent::SetVelocity(const FVector& Velocity) {
        if (!Velocity.IsFinite()) {
            return false;
        }

        mVelocity = Velocity;

        return true;
    }

    bool UMovementComponent::ShouldSkipUpdate(float DeltaTime) const {
        return !std::isfinite(DeltaTime) || DeltaTime <= 0.0f || GetUpdatedComponent() == nullptr;
    }

    bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool Sweep, Engine::FHitResult* OutHit) {
        return MoveUpdatedComponentInternal(Delta, NewRotation, Sweep, false, OutHit);
    }

    bool UMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool Sweep, Engine::FHitResult& OutHit) {
        return MoveUpdatedComponentInternal(Delta, NewRotation, Sweep, true, &OutHit);
    }

    bool UMovementComponent::SlideAlongSurface(const FVector& Delta, const Engine::FHitResult& Hit, Engine::FHitResult* OutHit) {
        if (Hit.mBlockingHit && !Hit.mStartPenetrating && Hit.mNormal.IsNormalized() && std::isfinite(Hit.mTime)) {
            const float RemainingTime{1.0f - std::clamp(Hit.mTime, 0.0f, 1.0f)};
            const FVector SlideDelta{(Delta - Hit.mNormal * Delta.Dot(Hit.mNormal)) * RemainingTime};
            const Engine::USceneComponent* Component{GetUpdatedComponent()};

            if (Component != nullptr && !SlideDelta.IsNearlyZero()) {
                return MoveUpdatedComponentInternal(SlideDelta, Component->GetTransform().mRotation, true, true, OutHit);
            }
        }

        if (OutHit != nullptr) {
            OutHit->Init();
        }

        return false;
    }

    void UMovementComponent::StopMovementImmediately() {
        mVelocity = {};
    }

    void UMovementComponent::RegisterComponentTickFunctions(bool Register) {
        if (Register) {
            UpdateTickEnabledState();
        } else {
            GetPrimaryComponentTick().SetTickEnabled(false);
        }

        Engine::UActorComponent::RegisterComponentTickFunctions(Register);
    }

    void UMovementComponent::UpdateTickEnabledState() {
        // 이동 컴포넌트가 대상보다 먼저 등록되어도 대상의 등록이 끝난 뒤 Tick할 수 있습니다.
        GetPrimaryComponentTick().SetTickEnabled(IsRegistered() && ResolveUpdatedComponent() != nullptr);
    }

}
