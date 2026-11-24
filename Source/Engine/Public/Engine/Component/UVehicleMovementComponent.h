/**
 * @file UVehicleMovementComponent.h
 * @brief Pawn의 조작 입력과 PhysX 차체를 연결하는 차량 이동 기반 클래스입니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/Math/FTransform.h"
#include "CoreUObject/TDelegate.h"
#include "Engine/Component/UPawnMovementComponent.h"
#include "UVehicleMovementComponent.generated.h"

namespace physx {
    class PxVehicleWheelsSimData;
}

namespace PhysicsRuntime {
    struct FBodyInstance;
    class FVehicleManager;
}

namespace Engine {

    /** @brief 차체 로컬 좌표와 미터·킬로그램 단위로 지정하는 바퀴 및 서스펜션 설정입니다. */
    USTRUCT()
    struct FVehicleWheelSettings {
        GENERATED_BODY()

        UPROPERTY(Edit, Serialize)
        FVector mCenterOffset{};

        UPROPERTY(Edit, Serialize)
        float mRadius{0.32f};

        UPROPERTY(Edit, Serialize)
        float mWidth{0.22f};

        UPROPERTY(Edit, Serialize)
        float mMass{20.0f};

        UPROPERTY(Edit, Serialize)
        float mMaxCompression{0.25f};

        UPROPERTY(Edit, Serialize)
        float mMaxDroop{0.2f};

        UPROPERTY(Edit, Serialize)
        float mSpringStrength{35000.0f};

        UPROPERTY(Edit, Serialize)
        float mDamperRate{4500.0f};

        bool operator==(const FVehicleWheelSettings&) const = default;
    };

    /** @brief 최근 서스펜션 업데이트에서 얻은 차체 로컬 자세와 접지 상태입니다. */
    struct FVehicleWheelState {
        FTransform mLocalPose{};
        float mSuspensionJounce{};
        bool mInAir{true};
    };

    /** @brief 등록된 강체의 수명에 맞춰 차량을 연결하고 고정 스텝에 조작 입력을 넘깁니다. */
    UCLASS(Abstract)
    class UVehicleMovementComponent : public Engine::UPawnMovementComponent {
        GENERATED_BODY()

    public:
        UVehicleMovementComponent();

        const Core::TArray<Engine::FVehicleWheelSettings>& GetWheelSettings() const;

        /** @brief 바퀴 설정을 저장하고 이미 연결된 차량을 갱신합니다. 빈 목록이면 차량 등록을 해제합니다. */
        bool SetWheelSettings(const Core::TArray<Engine::FVehicleWheelSettings>& Wheels);

        /** @brief 등록된 차량의 바퀴 결과를 값으로 복사합니다. */
        bool GetWheelState(Uint32 WheelIndex, Engine::FVehicleWheelState& OutState) const;

        void Tick(float DeltaTime) override;
        void EndPlay() override;
        bool SetTickGroup(Engine::ETickingGroup Group) override;
        void RegisterComponentTickFunctions(bool Register) override;

    protected:
        bool CanUseUpdatedComponent(const Engine::USceneComponent& Component) const override;
        void OnUpdatedComponentChanged(Engine::USceneComponent* Component) override;

        /** @brief 준비된 바퀴 설정으로 파생 클래스의 구동 방식을 장면에 등록합니다. */
        virtual bool CreateVehicle(PhysicsRuntime::FVehicleManager& Manager, const PhysicsRuntime::FBodyInstance& Body,
                                   const physx::PxVehicleWheelsSimData& Wheels) = 0;

        /** @brief PrePhysics에서 Pawn의 이번 입력을 차량에 전달합니다. */
        virtual void ApplyVehicleInput(PhysicsRuntime::FVehicleManager& Manager, const PhysicsRuntime::FBodyInstance& Body,
                                       const FVector& Input) = 0;

    private:
        class FPostPhysicsTick final : public Engine::FTickFunction {
        public:
            explicit FPostPhysicsTick(UVehicleMovementComponent& Target);

        private:
            const CoreUObject::UObject* GetTarget() const override;
            void ExecuteTick(float DeltaTime) override;

            UVehicleMovementComponent& mTarget;
        };

        PhysicsRuntime::FVehicleManager* GetVehicleManager() const;
        const PhysicsRuntime::FBodyInstance* GetRegisteredBody() const;
        void OnBodyStateChanged(Engine::UPrimitiveComponent* Chassis, bool Created);
        void TryRegisterVehicle();
        void UnregisterVehicle();
        void SynchronizeVelocity();

        UPROPERTY(Edit, Serialize, Getter = GetWheelSettings, Setter = SetWheelSettings)
        Core::TArray<Engine::FVehicleWheelSettings> mWheelSettings{};

        CoreUObject::TObjectRef<Engine::UPrimitiveComponent> mChassis{};
        CoreUObject::FDelegateHandle mBodyStateHandle{};
        bool mVehicleRegistered{};
        FPostPhysicsTick mPostPhysicsTick{*this};
    };

}
