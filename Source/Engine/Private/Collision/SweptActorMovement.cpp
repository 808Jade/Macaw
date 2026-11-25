#include "Engine/UWorld.h"

#include "Core/Literals/UnitLiterals.h"
#include "CoreUObject/Reflection/Cast.h"
#include "Engine/AActor.h"
#include "Engine/Collision/FHitResult.h"
#include "Engine/Component/UMovementComponent.h"
#include "Engine/Component/UShapeComponent.h"
#include "Engine/ULevel.h"

#include <algorithm>
#include <cmath>

using namespace Engine;

namespace {

    using namespace Core::Literals;
    constexpr auto SweepClearance{1_cm};

    struct FMovementSweepTargets {
        explicit FMovementSweepTargets(const Engine::UWorld& World, const Engine::AActor& Actor) {
            const Engine::ULevel* Level{World.GetPersistentLevel()};

            if (Level == nullptr) {
                return;
            }

            for (const auto& Other : Level->GetActors()) {
                if (Other.get() == &Actor || Other->GetWorld() != &World || Other->IsPendingDestroy()) {
                    continue;
                }

                for (const auto& Component : Other->GetComponents()) {
                    auto* Shape{CoreUObject::Cast<Engine::UShapeComponent>(Component.get())};

                    if (Shape != nullptr && Shape->BlocksMovement()) {
                        const auto& Pose{Shape->GetTransform()};
                        mShapes.emplace_back(Shape->GetCollisionShape(), Pose.mLocation, Pose.mRotation);
                        mComponents.push_back(Shape);
                    }
                }
            }
        }

        bool FindHit(const Engine::FCollisionShape& Shape, const FTransform& Pose, const FVector& Delta, bool ReportInitialPenetration, Engine::FHitResult& Hit) const {
            const auto Sweep{Engine::Collision::Sweep(Shape, Pose.mLocation, Delta, Pose.mRotation, mShapes, ReportInitialPenetration)};

            if (!Sweep.GetBlockingHit() || (Hit.mBlockingHit && Sweep.GetTime() >= Hit.mTime)) {
                return false;
            }

            const auto& Contact{Sweep.GetContact()};
            auto* Target{mComponents[Sweep.GetTargetIndex()]};
            Hit.SetActor(Target->GetOwner());
            Hit.SetComponent(Target);
            Hit.mBlockingHit = true;
            Hit.mStartPenetrating = Sweep.GetStartPenetrating();
            Hit.mTime = Sweep.GetTime();
            Hit.mImpactPoint = Contact.GetPoint();
            Hit.mNormal = Contact.GetNormal();
            Hit.mImpactNormal = Hit.mNormal;
            Hit.mPenetrationDepth = Contact.GetPenetrationDepth();

            return true;
        }

        Core::TArray<Engine::FSweepTarget> mShapes{};
        Core::TArray<Engine::UShapeComponent*> mComponents{};
    };

    void RetryAfterPenetration(const FMovementSweepTargets& Targets, const Engine::FCollisionShape& Shape, const FVector& Delta, FTransform& Pose, Engine::FHitResult& Hit) {
        if (!Hit.mNormal.IsNormalized() || !std::isfinite(Hit.mPenetrationDepth.GetBase()) || Hit.mPenetrationDepth <= 0_m) {
            return;
        }

        const auto Clearance{Hit.mPenetrationDepth + SweepClearance};
        const FVector Adjustment{Hit.mNormal * Clearance.GetBase()};
        FTransform AdjustedPose{Pose};
        AdjustedPose.mLocation += Adjustment;
        const FVector RetryEnd{AdjustedPose.mLocation + Delta};

        if (!AdjustedPose.mLocation.IsFinite() || !RetryEnd.IsFinite()) {
            return;
        }

        // 보정 경로는 빠져나오는 초기 접촉을 허용합니다. 보정 위치에서는 다시 초기 관통을 검사합니다.
        if (Engine::Collision::Sweep(Shape, Pose.mLocation, Adjustment, Pose.mRotation, Targets.mShapes).GetBlockingHit()) {
            return;
        }

        Engine::FHitResult RetryHit{};
        RetryHit.Init(AdjustedPose.mLocation, RetryEnd);
        Targets.FindHit(Shape, AdjustedPose, Delta, true, RetryHit);

        if (RetryHit.mStartPenetrating) {
            return;
        }

        Pose = AdjustedPose;
        Hit = RetryHit;
    }

    FVector GetSweepDisplacement(const FVector& Delta, Engine::FHitResult& Hit) {
        const auto Length{Core::FLength::FromBase(Delta.Length())};
        Hit.mDistance = Length * Hit.mTime;
        Hit.mLocation = Hit.mTraceStart + Delta * Hit.mTime;

        if (Hit.mBlockingHit && Length > 0_m) {
            const float ClearanceFraction{SweepClearance / Length};
            return Delta * std::max(0.0f, Hit.mTime - ClearanceFraction);
        }

        return Delta;
    }

    void DispatchBlockingHit(Engine::UShapeComponent& MovedShape, Engine::AActor& Actor, const Engine::FHitResult& Hit) {
        const auto MovedHandle{CoreUObject::MakeObjectRef(&MovedShape)};
        MovedShape.OnHit(Hit.GetActor(), Hit.GetComponent(), Hit);

        auto* Target{Hit.GetComponent()};
        auto* Moved{MovedHandle.Get()};

        if (Target != nullptr && Moved != nullptr) {
            const auto Reversed{Engine::FHitResult::GetReversedHit(Hit, Moved, &Actor)};
            Target->OnHit(&Actor, Moved, Reversed);
        }
    }

}

Engine::FHitResult UWorld::MoveActorSwept(Engine::AActor& Actor, const FVector& Delta) {
    Engine::FHitResult Hit{};
    if (Actor.GetWorld() != this || Actor.IsPendingDestroy() || !Delta.IsFinite() || Actor.GetRootComponent() == nullptr) {
        return Hit;
    }

    FDispatchScope Scope{this};
    auto Transform{Actor.GetActorTransform()};
    Hit.Init(Transform.mLocation, Transform.mLocation + Delta);
    if (Delta.IsNearlyZero()) {
        return Hit;
    }

    const FMovementSweepTargets Targets{*this, Actor};
    Engine::UShapeComponent* MovedShape{};
    for (const auto& Component : Actor.GetComponents()) {
        auto* Shape{CoreUObject::Cast<Engine::UShapeComponent>(Component.get())};

        if (Shape != nullptr && Shape->BlocksMovement() && Targets.FindHit(Shape->GetCollisionShape(), Shape->GetTransform(), Delta, false, Hit)) {
            MovedShape = Shape;
        }
    }

    Transform.mLocation += GetSweepDisplacement(Delta, Hit);
    if (!Actor.SetActorTransform(Transform)) {
        return {};
    }

    if (MovedShape != nullptr) {
        DispatchBlockingHit(*MovedShape, Actor, Hit);
    }

    return Hit;
}

bool UMovementComponent::MoveUpdatedComponentInternal(const FVector& Delta, const FQuat& NewRotation, bool Sweep, bool ResolvePenetration, Engine::FHitResult* OutHit) {
    auto* Component{GetUpdatedComponent()};
    FTransform Pose{Component != nullptr ? Component->GetTransform() : FTransform{}};
    const FVector End{Pose.mLocation + Delta};

    if (Component == nullptr || !End.IsFinite() || !NewRotation.IsNormalized()) {
        if (OutHit != nullptr) {
            OutHit->Init();
        }

        return false;
    }

    Engine::UWorld::FDispatchScope Scope{GetWorld()};
    Engine::FHitResult Hit{};
    Hit.Init(Pose.mLocation, End);

    auto* Actor{Component->GetOwner()};
    auto* Shape{Sweep ? CoreUObject::Cast<Engine::UShapeComponent>(Component) : nullptr};

    if (Shape != nullptr && Shape->BlocksMovement()) {
        const FMovementSweepTargets Targets{*GetWorld(), *Actor};
        const auto CollisionShape{Shape->GetCollisionShape()};
        Targets.FindHit(CollisionShape, Pose, Delta, true, Hit);

        if (Hit.mStartPenetrating && ResolvePenetration) {
            RetryAfterPenetration(Targets, CollisionShape, Delta, Pose, Hit);
        }
    }

    Pose.mLocation += GetSweepDisplacement(Delta, Hit);
    Pose.mRotation = NewRotation;
    bool Moved{};

    if (!Hit.mStartPenetrating) {
        Moved = Component == Actor->GetRootComponent() ? Actor->SetActorTransform(Pose) : Component->SetTransform(Pose);
    }

    if (OutHit != nullptr) {
        *OutHit = Hit;
    }

    if (Moved && Shape != nullptr && Hit.mBlockingHit) {
        DispatchBlockingHit(*Shape, *Actor, Hit);
    }

    return Moved;
}
