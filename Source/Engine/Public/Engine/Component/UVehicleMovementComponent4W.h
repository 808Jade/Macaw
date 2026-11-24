/**
 * @file UVehicleMovementComponent4W.h
 * @brief 엔진, 자동 변속기, 차동장치가 있는 4륜 구동 차량입니다.
 * @ingroup Engine
 */

#pragma once

#include "Engine/Component/UVehicleMovementComponent.h"
#include "UVehicleMovementComponent4W.generated.h"

namespace Engine {

    /** @brief 바퀴는 전좌·전우·후좌·후우 순서이며 앞바퀴가 조향합니다. */
    UCLASS()
    class UVehicleMovementComponent4W : public Engine::UVehicleMovementComponent {
        GENERATED_BODY()

    public:
        /** @brief 가속과 조향은 [-1, 1], 제동은 [0, 1]입니다. 음수 가속은 후진을 요청합니다. */
        bool SetDriveInput(float Throttle, float Brake, float Steering);

        /** @brief 차체 위치를 보존하면서 속도와 구동계 상태를 초기화합니다. */
        bool ResetDrive();

        /** @brief 후진은 -1, 중립은 0, 전진은 1부터 시작합니다. */
        Int32 GetCurrentGear() const;

        /** @brief 차체 전방 기준의 부호 있는 속도(m/s)입니다. */
        Core::FSpeed GetForwardSpeed() const;

    protected:
        bool CreateVehicle(PhysicsRuntime::FVehicleManager& Manager, const PhysicsRuntime::FBodyInstance& Body,
                           const physx::PxVehicleWheelsSimData& Wheels) override;
    };

}
