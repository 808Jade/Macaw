/**
 * @file UMovementComponent.h
 * @brief 공간 컴포넌트의 이동 대상과 월드 속도를 관리합니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/Math/FVector.h"
#include "CoreUObject/TObjectRef.h"
#include "Engine/Component/UActorComponent.h"
#include "UMovementComponent.generated.h"

namespace Engine {

    class USceneComponent;
    class UPrimitiveComponent;

    /** @brief 이동 대상 지정과 월드 공간 이동을 제공하는 컴포넌트입니다. */
    UCLASS()
    class UMovementComponent : public Engine::UActorComponent {
        GENERATED_BODY()

    public:
        UMovementComponent();

        /** @brief 명시적 대상을 지정합니다. nullptr을 전달하면 루트 자동 선택도 해제합니다. */
        virtual void SetUpdatedComponent(Engine::USceneComponent* Component);

        /** @brief 등록된 월드에서 사용할 수 있는 이동 대상을 반환합니다. */
        Engine::USceneComponent* GetUpdatedComponent() const;

        /** @brief 이동 대상이 프리미티브이면 해당 컴포넌트를 반환합니다. */
        Engine::UPrimitiveComponent* GetUpdatedPrimitive() const;

        const FVector& GetVelocity() const;
        bool SetVelocity(const FVector& Velocity);

        /** @brief 유효한 시간 간격과 이동 대상이 있는지 확인합니다. */
        bool ShouldSkipUpdate(float DeltaTime) const;

        /** @brief 월드 공간의 위치 변화량과 새 회전을 대상에 적용합니다. */
        bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation);

        /** @brief 현재 월드 속도를 0으로 만듭니다. */
        virtual void StopMovementImmediately();

        void RegisterComponentTickFunctions(bool Register) override;

    private:
        friend class AActor;

        Engine::USceneComponent* ResolveUpdatedComponent() const;
        void UpdateTickEnabledState();
        void SetExplicitUpdatedComponent(const CoreUObject::TObjectRef<Engine::USceneComponent>& Component);

    private:
        /** @brief 처음에는 소유 액터의 현재 루트를 이동 대상으로 사용합니다. */
        UPROPERTY(Serialize)
        bool mUseOwnerRoot{true};

        /** @brief 명시적 대상의 GUID와 로드 후 연결된 핸들을 보관합니다. */
        UPROPERTY(Serialize, Name = "UpdatedComponent", Setter = SetExplicitUpdatedComponent)
        CoreUObject::TObjectRef<Engine::USceneComponent> mExplicitUpdatedComponent{};

        FVector mVelocity{};
    };

}
