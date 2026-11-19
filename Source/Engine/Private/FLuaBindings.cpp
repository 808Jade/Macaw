#include "FLuaRuntime.h"

#include "Core/Math/FRotator.h"
#include "Core/Math/FQuat.h"
#include "Core/Math/FTransform.h"
#include "Engine/AActor.h"
#include "Engine/Component/UCameraComponent.h"
#include "Engine/Component/UProjectileMovementComponent.h"
#include "Engine/Component/USkeletalMeshComponent.h"
#include "Engine/FWorldHit.h"
#include "Engine/UWorld.h"

#include <iostream>
#include <tuple>

using namespace Engine;

namespace {

    Engine::UActorComponent* FindComponent(const Engine::AActor& Actor, const CoreUObject::UClass* Class) {
        for (const auto& Component : Actor.GetComponents()) {
            if (Component->IsA(Class)) {
                return Component.get();
            }
        }
        return nullptr;
    }

    Engine::UProjectileMovementComponent& ResolveMovement(const Engine::AActor& Actor) {
        if (auto* Movement{static_cast<Engine::UProjectileMovementComponent*>(FindComponent(Actor, Engine::UProjectileMovementComponent::StaticClass()))}) {
            return *Movement;
        }
        throw sol::error{"Actor needs a Projectile Movement component for Velocity"};
    }

}

AActor& FLuaRuntime::FActorRef::Resolve() const {
    auto* Actor{mActor.Get()};
    if (Actor == nullptr || Actor->IsPendingDestroy()) {
        throw sol::error{"Actor is no longer valid"};
    }
    return *Actor;
}

USkeletalMeshComponent& FLuaRuntime::FSkeletalMeshRef::Resolve() const {
    auto* Component{mComponent.Get()};
    const auto* Owner{Component != nullptr ? Component->GetOwner() : nullptr};
    if (Owner == nullptr || Owner->IsPendingDestroy()) {
        throw sol::error{"Skeletal mesh component is no longer valid"};
    }
    return *Component;
}

void FLuaRuntime::RegisterBindings() {
    auto Vector{mLua.new_usertype<FVector>("Vector", sol::constructors<FVector(), FVector(float, float, float)>())};
    Vector["x"] = &FVector::x;
    Vector["y"] = &FVector::y;
    Vector["z"] = &FVector::z;
    Vector["Length"] = &FVector::Length;
    Vector["LengthSquared"] = &FVector::LengthSquared;
    Vector["Normalized"] = &FVector::Normalized;
    Vector[sol::meta_function::addition] = [](const FVector& Left, const FVector& Right) {
        return Left + Right;
    };
    Vector[sol::meta_function::subtraction] = [](const FVector& Left, const FVector& Right) {
        return Left - Right;
    };
    Vector[sol::meta_function::multiplication] = sol::overload([](const FVector& Value, float Scalar) {
        return Value * Scalar;
    }, [](float Scalar, const FVector& Value) {
        return Value * Scalar;
    });

    auto Rotator{mLua.new_usertype<FRotator>("Rotator", sol::constructors<FRotator(), FRotator(float, float, float)>())};
    Rotator["Pitch"] = &FRotator::pitch;
    Rotator["Yaw"] = &FRotator::yaw;
    Rotator["Roll"] = &FRotator::roll;

    auto Quat{mLua.new_usertype<FQuat>("Quat", sol::constructors<FQuat(), FQuat(const FVector&, float)>())};
    Quat["Normalized"] = &FQuat::Normalized;
    Quat["RotateVector"] = &FQuat::RotateVector;
    Quat[sol::meta_function::multiplication] = [](const FQuat& Left, const FQuat& Right) {
        return Left * Right;
    };

    auto Transform{mLua.new_usertype<FTransform>("Transform", sol::constructors<FTransform()>())};
    Transform["Location"] = &FTransform::mLocation;
    Transform["Rotation"] = &FTransform::mRotation;
    Transform["Scale"] = &FTransform::mScale;
    Transform["GetRotator"] = &FTransform::GetRotator;
    Transform["SetRotator"] = &FTransform::SetRotator;

    auto Hit{mLua.new_usertype<Engine::FHitResult>("HitResult", sol::no_constructor)};
    Hit["BlockingHit"] = sol::readonly(&Engine::FHitResult::mBlockingHit);
    Hit["ImpactPoint"] = sol::readonly(&Engine::FHitResult::mImpactPoint);
    Hit["Normal"] = sol::readonly(&Engine::FHitResult::mNormal);
    Hit["Time"] = sol::readonly(&Engine::FHitResult::mTime);

    auto RaycastHit{mLua.new_usertype<Engine::FWorldHit>("RaycastHit", sol::no_constructor)};
    RaycastHit["IsHit"] = sol::property([](const Engine::FWorldHit& Value) {
        return Value.mActor.Get() != nullptr;
    });
    RaycastHit["Position"] = sol::readonly(&Engine::FWorldHit::mPosition);
    RaycastHit["Distance"] = sol::readonly(&Engine::FWorldHit::mDistance);

    auto SkeletalMesh{mLua.new_usertype<FSkeletalMeshRef>("SkeletalMeshComponent", sol::no_constructor)};
    SkeletalMesh["SetAnimGraphFloat"] = [](const FSkeletalMeshRef& Ref, const Core::FString& Name, float Value) {
        return Ref.Resolve().SetAnimGraphFloat(Name, Value);
    };
    SkeletalMesh["SetAnimGraphBool"] = [](const FSkeletalMeshRef& Ref, const Core::FString& Name, bool Value) {
        return Ref.Resolve().SetAnimGraphBool(Name, Value);
    };
    SkeletalMesh["RestartAnimGraphPlayer"] = [](const FSkeletalMeshRef& Ref, const Core::FString& Name) {
        return Ref.Resolve().RestartAnimGraphPlayer(Name);
    };
    SkeletalMesh["IsAnimGraphPlayerFinished"] = [](const FSkeletalMeshRef& Ref, const Core::FString& Name) {
        return Ref.Resolve().IsAnimGraphPlayerFinished(Name);
    };

    auto Actor{mLua.new_usertype<FActorRef>("Actor", sol::no_constructor)};
    Actor["IsValid"] = [](const FActorRef& Ref) {
        const auto* Target{Ref.mActor.Get()};
        return Target != nullptr && !Target->IsPendingDestroy();
    };
    Actor["GetName"] = [](const FActorRef& Ref) {
        return Ref.Resolve().GetName();
    };
    Actor["UUID"] = sol::property([](const FActorRef& Ref) {
        return Ref.mActor.GetGuid().ToString();
    });
    Actor["GetActorTransform"] = [](const FActorRef& Ref) {
        return Ref.Resolve().GetActorTransform();
    };
    Actor["SetActorTransform"] = [](const FActorRef& Ref, const FTransform& Value) {
        return Ref.Resolve().SetActorTransform(Value);
    };
    Actor["GetSkeletalMeshComponent"] = [](const FActorRef& Ref) -> sol::optional<FSkeletalMeshRef> {
        if (auto* Component{static_cast<Engine::USkeletalMeshComponent*>(FindComponent(Ref.Resolve(), Engine::USkeletalMeshComponent::StaticClass()))}) {
            return FSkeletalMeshRef{CoreUObject::MakeObjectRef(Component)};
        }
        return sol::nullopt;
    };
    Actor["Location"] = sol::property([](const FActorRef& Ref) {
        return Ref.Resolve().GetActorTransform().mLocation;
    }, [](const FActorRef& Ref, const FVector& Value) {
        auto& Target{Ref.Resolve()};
        auto Pose{Target.GetActorTransform()};
        Pose.mLocation = Value;
        if (!Target.SetActorTransform(Pose)) {
            throw sol::error{"Invalid actor transform"};
        }
    });
    Actor["Velocity"] = sol::property([](const FActorRef& Ref) {
        return ResolveMovement(Ref.Resolve()).GetVelocity();
    }, [](const FActorRef& Ref, const FVector& Value) {
        if (!ResolveMovement(Ref.Resolve()).SetVelocity(Value)) {
            throw sol::error{"Invalid movement velocity"};
        }
    });
    Actor["PrintLocation"] = [](const FActorRef& Ref) {
        const auto& Target{Ref.Resolve()};
        const auto Location{Target.GetActorTransform().mLocation};
        std::clog << Target.GetName() << " Location " << Location.x << ' ' << Location.y << ' ' << Location.z << '\n';
    };
    Actor["MoveSwept"] = [](const FActorRef& Ref, const FVector& Delta) {
        auto& Target{Ref.Resolve()};
        if (Target.GetWorld() == nullptr || !Delta.IsFinite()) {
            throw sol::error{"Invalid sweep movement"};
        }
        return Target.GetWorld()->MoveActorSwept(Target, Delta);
    };

    auto World{mLua.create_named_table("World")};
    World["FindActor"] = [this](const Core::FString& Name) -> sol::optional<FActorRef> {
        if (auto* Found{mWorld.FindActor(Name)}) {
            return FActorRef{CoreUObject::MakeObjectRef(Found)};
        }
        return sol::nullopt;
    };
    World["Raycast"] = [this](const FVector& Origin, const FVector& Direction, const sol::optional<FActorRef>& Ignored) {
        return mWorld.Raycast(Origin, Direction, Ignored ? &Ignored->Resolve() : nullptr);
    };
    World["SetActiveCamera"] = [this](const FActorRef& Ref) {
        auto* Camera{static_cast<Engine::UCameraComponent*>(FindComponent(Ref.Resolve(), Engine::UCameraComponent::StaticClass()))};
        return Camera != nullptr && mWorld.SetActiveCamera(Camera);
    };

    mLua.new_enum<Core::EKey>("Key", {{"W", Core::EKey::W}, {"A", Core::EKey::A}, {"S", Core::EKey::S}, {"D", Core::EKey::D}, {"C", Core::EKey::C}, {"V", Core::EKey::V}, {"F", Core::EKey::F}, {"Q", Core::EKey::Q}, {"R", Core::EKey::R}, {"E", Core::EKey::E}, {"X", Core::EKey::X}, {"K", Core::EKey::K}, {"H", Core::EKey::H}, {"Z", Core::EKey::Z}, {"Space", Core::EKey::Space}, {"Backspace", Core::EKey::Backspace}, {"Delete", Core::EKey::Delete}, {"Shift", Core::EKey::Shift}, {"LeftShift", Core::EKey::LeftShift}, {"RightShift", Core::EKey::RightShift}, {"MouseLeft", Core::EKey::MouseLeft}, {"MouseRight", Core::EKey::MouseRight}});

    auto Input{mLua.create_named_table("Input")};
    Input["IsDown"] = [this](Core::EKey Key) {
        return mFrameInput.IsDown(Key, Core::EInputRoute::Game);
    };
    Input["WasPressed"] = [this](Core::EKey Key) {
        return mFrameInput.WasPressed(Key, Core::EInputRoute::Game);
    };
    Input["GetMouseDelta"] = [this] {
        const auto Delta{mFrameInput.GetMouseDelta(Core::EInputRoute::Game)};
        return std::tuple{Delta.x, Delta.y};
    };
}
