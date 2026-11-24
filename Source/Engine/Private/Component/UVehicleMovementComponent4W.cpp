/**
 * @file UVehicleMovementComponent4W.cpp
 * @brief Pawn에서 4륜 구동계에 페달과 조향 입력을 전달합니다.
 * @ingroup Engine
 */

#include "Engine/Component/UVehicleMovementComponent4W.h"
#include "Engine/Component/UPrimitiveComponent.h"
#include "PhysicsRuntime/FVehicleManager.h"

namespace Engine {

    bool UVehicleMovementComponent4W::SetDriveInput(float Throttle, float Brake, float Steering) {
        const auto* Body{GetRegisteredBody()};
        auto* Manager{GetVehicleManager()};

        return Body != nullptr && Manager != nullptr && Manager->SetDrive4WInput(*Body, Throttle, Brake, Steering);
    }

    bool UVehicleMovementComponent4W::ResetDrive() {
        const auto* Body{GetRegisteredBody()};
        auto* Manager{GetVehicleManager()};

        if (Body == nullptr || Manager == nullptr || !Manager->ResetDrive4W(*Body)) {
            return false;
        }

        SetVelocity({});
        return true;
    }

    Int32 UVehicleMovementComponent4W::GetCurrentGear() const {
        const auto* Body{GetRegisteredBody()};
        const auto* Manager{GetVehicleManager()};

        return Body != nullptr && Manager != nullptr ? Manager->GetDrive4WGear(*Body) : 0;
    }

    Core::FSpeed UVehicleMovementComponent4W::GetForwardSpeed() const {
        const auto* Chassis{GetUpdatedPrimitive()};
        if (Chassis == nullptr) {
            return {};
        }

        const auto Forward{Chassis->GetTransform().mRotation.RotateVector({1.0f, 0.0f, 0.0f})};
        return Core::FSpeed::FromBase(GetVelocity().Dot(Forward));
    }

    bool UVehicleMovementComponent4W::CreateVehicle(PhysicsRuntime::FVehicleManager& Manager, const PhysicsRuntime::FBodyInstance& Body,
                                                   const physx::PxVehicleWheelsSimData& Wheels) {
        return Manager.RegisterDrive4W(Body, Wheels);
    }

}
