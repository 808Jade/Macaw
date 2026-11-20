#include "Engine/UWorld.h"

#include "CoreUObject/Reflection/Cast.h"
#include "Engine/AActor.h"
#include "Engine/Collision/FHitResult.h"
#include "Engine/Component/UShapeComponent.h"

#include <algorithm>

using namespace Engine;

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

    Core::TArray<Engine::FSweepTarget> Targets{};
    Core::TArray<Engine::UShapeComponent*> TargetShapes{};
    for (const auto& Handle : GetActorRefs()) {
        auto* Other{ResolveActor(Handle)};
        if (Other == nullptr || Other == &Actor || Other->IsPendingDestroy()) {
            continue;
        }
        for (const auto& Component : Other->GetComponents()) {
            auto* Shape{CoreUObject::Cast<Engine::UShapeComponent>(Component.get())};
            if (Shape != nullptr && Shape->BlocksMovement()) {
                const auto& Pose{Shape->GetTransform()};
                Targets.emplace_back(Shape->GetCollisionShape(), Pose.mLocation, Pose.mRotation);
                TargetShapes.push_back(Shape);
            }
        }
    }

    Engine::UShapeComponent* MovedShape{};
    for (const auto& Component : Actor.GetComponents()) {
        auto* Shape{CoreUObject::Cast<Engine::UShapeComponent>(Component.get())};
        if (Shape == nullptr || !Shape->BlocksMovement() || Targets.empty()) {
            continue;
        }

        const auto& Pose{Shape->GetTransform()};
        const auto Sweep{Engine::Collision::Sweep(Shape->GetCollisionShape(), Pose.mLocation, Delta, Pose.mRotation, Targets)};
        if (!Sweep.GetBlockingHit() || (Hit.mBlockingHit && Sweep.GetTime() >= Hit.mTime)) {
            continue;
        }

        const auto& Contact{Sweep.GetContact()};
        auto* Target{TargetShapes[Sweep.GetTargetIndex()]};
        MovedShape = Shape;
        Hit.SetActor(Target->GetOwner());
        Hit.SetComponent(Target);
        Hit.mBlockingHit = true;
        Hit.mStartPenetrating = Sweep.GetStartPenetrating();
        Hit.mTime = Sweep.GetTime();
        Hit.mImpactPoint = Contact.GetPoint();
        Hit.mNormal = Contact.GetNormal();
        Hit.mImpactNormal = Hit.mNormal;
        Hit.mPenetrationDepth = Contact.GetPenetrationDepth();
    }

    const float Length{Delta.Length()};
    Hit.mDistance = Length * Hit.mTime;
    Hit.mLocation = Transform.mLocation + Delta * Hit.mTime;
    const float MoveTime{Hit.mBlockingHit ? std::max(0.0f, Hit.mTime - 0.01f / Length) : 1.0f};
    Transform.mLocation += Delta * MoveTime;
    if (!Actor.SetActorTransform(Transform)) {
        return {};
    }

    if (MovedShape != nullptr) {
        const auto MovedHandle{CoreUObject::MakeObjectRef(MovedShape)};
        MovedShape->OnHit(Hit.GetActor(), Hit.GetComponent(), Hit);
        if (auto* Target{Hit.GetComponent()}; Target != nullptr && MovedHandle.Get() != nullptr) {
            const auto Reversed{Engine::FHitResult::GetReversedHit(Hit, MovedHandle.Get(), &Actor)};
            Target->OnHit(&Actor, MovedHandle.Get(), Reversed);
        }
    }
    return Hit;
}
