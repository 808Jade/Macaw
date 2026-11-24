/**
 * @file UVehicleMovementComponent.cpp
 * @brief 차체 수명, 차량 입력과 물리 결과를 연결합니다.
 * @ingroup Engine
 */

#include "Engine/Component/UVehicleMovementComponent.h"
#include "CoreUObject/Reflection/Cast.h"
#include "Engine/AActor.h"
#include "Engine/Component/UPrimitiveComponent.h"
#include "Engine/UWorld.h"
#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/FVehicleManager.h"
#include "PhysicsRuntime/PhysicsConversion.h"

#include <vehicle/PxVehicleUpdate.h>
#include <vehicle/PxVehicleUtilSetup.h>
#include <vehicle/PxVehicleWheels.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace Engine {

    namespace {
        bool IsPositiveFinite(float Value) {
            return std::isfinite(Value) && Value > 0.0f;
        }

        float ComputeWheelInertia(const FVehicleWheelSettings& Wheel) {
            return 0.5f * Wheel.mMass * Wheel.mRadius * Wheel.mRadius;
        }

        bool IsValidWheel(const FVehicleWheelSettings& Wheel) {
            return Wheel.mCenterOffset.IsFinite() && IsPositiveFinite(Wheel.mRadius) && IsPositiveFinite(Wheel.mWidth) &&
                   IsPositiveFinite(Wheel.mMass) && IsPositiveFinite(ComputeWheelInertia(Wheel)) &&
                   IsPositiveFinite(Wheel.mMaxCompression) && IsPositiveFinite(Wheel.mMaxDroop) &&
                   IsPositiveFinite(Wheel.mSpringStrength) && std::isfinite(Wheel.mDamperRate) && Wheel.mDamperRate >= 0.0f;
        }

        bool ConfigureWheels(physx::PxVehicleWheelsSimData& Data, const Core::TArray<FVehicleWheelSettings>& Settings,
                             const physx::PxRigidDynamic& Chassis) {
            const auto Count{Data.getNbWheels()};
            const auto CenterOfMass{Chassis.getCMassLocalPose()};
            std::array<physx::PxVec3, PX_MAX_NB_WHEELS> WheelCenters{};
            std::array<physx::PxReal, PX_MAX_NB_WHEELS> SprungMasses{};

            for (physx::PxU32 Wheel{}; Wheel < Count; ++Wheel) {
                WheelCenters[Wheel] = PhysicsRuntime::ToPxVec3(Settings[Wheel].mCenterOffset);
            }

            constexpr physx::PxU32 GravityAxisZ{2};
            physx::PxVehicleComputeSprungMasses(Count, WheelCenters.data(), CenterOfMass.p, Chassis.getMass(), GravityAxisZ, SprungMasses.data());

            if (!std::all_of(SprungMasses.begin(), SprungMasses.begin() + Count, IsPositiveFinite)) {
                return false;
            }

            for (physx::PxU32 Wheel{}; Wheel < Count; ++Wheel) {
                const FVehicleWheelSettings& Setting{Settings[Wheel]};
                physx::PxVehicleWheelData WheelData{};
                WheelData.mRadius = Setting.mRadius;
                WheelData.mWidth = Setting.mWidth;
                WheelData.mMass = Setting.mMass;
                WheelData.mMOI = ComputeWheelInertia(Setting);

                physx::PxVehicleSuspensionData Suspension{};
                Suspension.mMaxCompression = Setting.mMaxCompression;
                Suspension.mMaxDroop = Setting.mMaxDroop;
                Suspension.mSpringStrength = Setting.mSpringStrength;
                Suspension.mSpringDamperRate = Setting.mDamperRate;
                Suspension.mSprungMass = SprungMasses[Wheel];

                const auto Offset{CenterOfMass.transformInv(WheelCenters[Wheel])};
                Data.setWheelData(Wheel, WheelData);
                Data.setSuspensionData(Wheel, Suspension);
                Data.setWheelCentreOffset(Wheel, Offset);
                Data.setSuspTravelDirection(Wheel, CenterOfMass.q.rotateInv({0.0f, 0.0f, -1.0f}));
                Data.setSuspForceAppPointOffset(Wheel, Offset);
                Data.setTireForceAppPointOffset(Wheel, Offset);

                // 서스펜션 레이와 별도로 차체의 기존 충돌 형상을 보존합니다.
                Data.setWheelShapeMapping(Wheel, -1);
            }

            return true;
        }
    }

    UVehicleMovementComponent::UVehicleMovementComponent() = default;

    const Core::TArray<FVehicleWheelSettings>& UVehicleMovementComponent::GetWheelSettings() const {
        return mWheelSettings;
    }

    bool UVehicleMovementComponent::SetWheelSettings(const Core::TArray<FVehicleWheelSettings>& Wheels) {
        if (Wheels.size() > PX_MAX_NB_WHEELS || !std::ranges::all_of(Wheels, IsValidWheel)) {
            return false;
        }

        if (mWheelSettings != Wheels) {
            UnregisterVehicle();
            mWheelSettings = Wheels;
            TryRegisterVehicle();
        }

        return true;
    }

    bool UVehicleMovementComponent::GetWheelState(Uint32 WheelIndex, FVehicleWheelState& OutState) const {
        const auto* Body{GetRegisteredBody()};
        const auto* Manager{GetVehicleManager()};

        if (Body == nullptr || Manager == nullptr) {
            return false;
        }

        const auto Results{Manager->GetWheelQueryResults(*Body)};
        if (WheelIndex >= Results.size()) {
            return false;
        }

        const auto& Wheel{Results[WheelIndex]};
        OutState = {PhysicsRuntime::ToFTransform(Wheel.localPose, {1.0f, 1.0f, 1.0f}), Wheel.suspJounce, Wheel.isInAir};

        return true;
    }

    void UVehicleMovementComponent::Tick(float DeltaTime) {
        if (!HasBegunPlay() || ShouldSkipUpdate(DeltaTime)) {
            return;
        }

        const FVector Input{ConsumeInputVector()};
        const auto* Body{GetRegisteredBody()};
        auto* Manager{GetVehicleManager()};

        if (Body != nullptr && Manager != nullptr) {
            ApplyVehicleInput(*Manager, *Body, Input);
        }
    }

    void UVehicleMovementComponent::EndPlay() {
        UnregisterVehicle();
        UPawnMovementComponent::EndPlay();
    }

    bool UVehicleMovementComponent::SetTickGroup(Engine::ETickingGroup Group) {
        return Group == Engine::ETickingGroup::PrePhysics && UPawnMovementComponent::SetTickGroup(Group);
    }

    void UVehicleMovementComponent::RegisterComponentTickFunctions(bool Register) {
        UPawnMovementComponent::RegisterComponentTickFunctions(Register);

        if (!Register) {
            mPostPhysicsTick.UnregisterTickFunction();
        } else if (auto* Level{GetOwner() != nullptr ? GetOwner()->GetLevel() : nullptr}) {
            mPostPhysicsTick.RegisterTickFunction(*Level);
        }
    }

    bool UVehicleMovementComponent::CanUseUpdatedComponent(const Engine::USceneComponent& Component) const {
        return UPawnMovementComponent::CanUseUpdatedComponent(Component) && CoreUObject::Cast<Engine::UPrimitiveComponent>(&Component) != nullptr;
    }

    void UVehicleMovementComponent::OnUpdatedComponentChanged(Engine::USceneComponent* Component) {
        auto* Chassis{CoreUObject::Cast<Engine::UPrimitiveComponent>(Component)};
        const auto Target{CoreUObject::MakeObjectRef(Chassis)};

        if (mChassis == Target) {
            return;
        }

        UnregisterVehicle();

        if (auto* Previous{mChassis.Get()}) {
            Previous->mOnBodyStateChanged.Remove(mBodyStateHandle);
        }

        mChassis = Target;
        mBodyStateHandle = {};

        if (Chassis != nullptr) {
            mBodyStateHandle = Chassis->mOnBodyStateChanged.AddDynamic(this, &UVehicleMovementComponent::OnBodyStateChanged);
            TryRegisterVehicle();
        }
    }

    void UVehicleMovementComponent::OnBodyStateChanged(Engine::UPrimitiveComponent* Chassis, bool Created) {
        if (Chassis != mChassis.Get()) {
            return;
        }

        if (Created) {
            TryRegisterVehicle();
        } else {
            UnregisterVehicle();
        }
    }

    PhysicsRuntime::FVehicleManager* UVehicleMovementComponent::GetVehicleManager() const {
        const auto* World{GetWorld()};
        const auto* Scene{World != nullptr ? World->GetPhysicsScene() : nullptr};

        return Scene != nullptr ? Scene->GetVehicleManager() : nullptr;
    }

    const PhysicsRuntime::FBodyInstance* UVehicleMovementComponent::GetRegisteredBody() const {
        const auto* Chassis{mVehicleRegistered ? GetUpdatedPrimitive() : nullptr};

        return Chassis != nullptr ? &Chassis->GetBodyInstance() : nullptr;
    }

    void UVehicleMovementComponent::TryRegisterVehicle() {
        if (mVehicleRegistered || mWheelSettings.empty()) {
            return;
        }

        const auto* Chassis{GetUpdatedPrimitive()};
        auto* Manager{GetVehicleManager()};
        const auto* Body{Chassis != nullptr ? &Chassis->GetBodyInstance() : nullptr};
        const auto* Actor{Body != nullptr ? Body->GetPxRigidDynamic() : nullptr};

        if (Manager == nullptr || Actor == nullptr || Actor->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC)) {
            return;
        }

        std::unique_ptr<physx::PxVehicleWheelsSimData, decltype([](auto* Data) { Data->free(); })> Wheels{
            physx::PxVehicleWheelsSimData::allocate(static_cast<physx::PxU32>(mWheelSettings.size()))};

        if (Wheels != nullptr && ConfigureWheels(*Wheels, mWheelSettings, *Actor) && CreateVehicle(*Manager, *Body, *Wheels)) {
            mVehicleRegistered = true;
            mPostPhysicsTick.SetTickEnabled(true);
            SynchronizeVelocity();
        }
    }

    void UVehicleMovementComponent::UnregisterVehicle() {
        if (mVehicleRegistered) {
            mVehicleRegistered = false;

            const auto* Chassis{mChassis.Get()};
            if (auto* Manager{GetVehicleManager()}; Manager != nullptr && Chassis != nullptr) {
                Manager->UnregisterVehicle(Chassis->GetBodyInstance());
            }
        }

        mPostPhysicsTick.SetTickEnabled(false);
        UMovementComponent::SetVelocity({});
    }

    void UVehicleMovementComponent::SynchronizeVelocity() {
        const auto* Body{GetRegisteredBody()};
        UMovementComponent::SetVelocity(Body != nullptr ? Body->GetLinearVelocity() : FVector{});
    }

    UVehicleMovementComponent::FPostPhysicsTick::FPostPhysicsTick(UVehicleMovementComponent& Target)
        : mTarget{Target} {
        SetTickGroup(Engine::ETickingGroup::PostPhysics);
        SetTickEnabled(false);
    }

    const CoreUObject::UObject* UVehicleMovementComponent::FPostPhysicsTick::GetTarget() const {
        return &mTarget;
    }

    void UVehicleMovementComponent::FPostPhysicsTick::ExecuteTick(float) {
        mTarget.SynchronizeVelocity();
    }

}
