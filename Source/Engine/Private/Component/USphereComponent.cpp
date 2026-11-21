/**
 * @file USphereComponent.cpp
 * @brief 로컬 반지름으로 정의한 구 충돌 컴포넌트입니다.
 * @ingroup Engine
 */

#include "Engine/Component/USphereComponent.h"

#include <algorithm>

namespace Engine {

    /**
     * @brief 검사에 사용할 충돌 형상 값을 반환합니다.
     */
    Engine::FCollisionShape Engine::USphereComponent::GetCollisionShape() const {
        return Engine::FCollisionShape::MakeSphere(GetScaledSphereRadius());
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

    /**
     * @brief 현재 반지름으로 설계도의 구 모양을 다시 채웁니다.
     */
    void Engine::USphereComponent::UpdateBodySetup() {
        constexpr float KindaSmallNumber{1.e-4f};
        constexpr float MinimumRadius{0.001f};

        // 반지름이 0이면 아주 작은 반지름으로 바꿔 모양을 만들 수 있게 합니다.
        const float Radius{mSphereRadius < KindaSmallNumber ? MinimumRadius : mSphereRadius};

        mShapeBodySetup->mAggGeom = {};
        mShapeBodySetup->mAggGeom.mSphereElems.push_back(PhysicsRuntime::FKSphereElem{Radius});
    }

}
