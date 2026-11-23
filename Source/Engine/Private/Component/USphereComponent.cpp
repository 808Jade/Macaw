/**
 * @file USphereComponent.cpp
 * @brief 로컬 반지름으로 정의한 구 충돌 컴포넌트입니다.
 * @ingroup Engine
 */

#include "Engine/Component/USphereComponent.h"
#include "Engine/FPrimitiveDrawInterface.h"
#include "Core/Literals.h"

#include <algorithm>

using namespace Core::Literals;

namespace Engine {

    void Engine::USphereComponent::DrawShape(Engine::FPrimitiveDrawInterface& DrawInterface, const FVector& Color) const {
        const FVector Center{GetTransform().mLocation};
        const FQuat& Rot{GetTransform().mRotation};
        const FVector F{Rot.ForwardVector()};
        const FVector Rt{Rot.RightVector()};
        const FVector Up{Rot.UpVector()};
        const float R{GetScaledSphereRadius()};

        DrawInterface.DrawArc(Center, F, Rt, R, 0_rad, 1_turn, Color);
        DrawInterface.DrawArc(Center, F, Up, R, 0_rad, 1_turn, Color);
        DrawInterface.DrawArc(Center, Rt, Up, R, 0_rad, 1_turn, Color);
    }

    /**
     * @brief 충돌 형상의 로컬 축 정렬 경계를 계산합니다.
     */
    FBox Engine::USphereComponent::CalcLocalBounds() const {
        return FBox{ FVector{ -mSphereRadius, -mSphereRadius, -mSphereRadius }, FVector{ mSphereRadius, mSphereRadius, mSphereRadius } };
    }

    /**
     * @brief 구의 로컬 반지름을 설정합니다.
     */
    void Engine::USphereComponent::SetSphereRadius(float InRadius) {
        mSphereRadius = std::max(0.0f, InRadius);
    }

    /**
     * @brief 월드 스케일을 반영한 구 반지름을 반환합니다.
     */
    float Engine::USphereComponent::GetScaledSphereRadius() const {
        return mSphereRadius * GetShapeScale();
    }

    /**
     * @brief 구의 로컬 반지름을 반환합니다.
     */
    float Engine::USphereComponent::GetSphereRadius() const {
        return mSphereRadius;
    }

}
