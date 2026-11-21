/**
 * @file UBoxComponent.cpp
 * @brief 로컬 반길이로 정의한 상자 충돌 컴포넌트입니다.
 * @ingroup Engine
 */

#include "Engine/Component/UBoxComponent.h"

#include <algorithm>
#include <cmath>

namespace Engine {

    /**
     * @brief 검사에 사용할 충돌 형상 값을 반환합니다.
     */
    Engine::FCollisionShape Engine::UBoxComponent::GetCollisionShape() const {
        return Engine::FCollisionShape::MakeBox(GetScaledBoxExtent());
    }

    /**
     * @brief 충돌 형상의 로컬 축 정렬 경계를 계산합니다.
     */
    FBox Engine::UBoxComponent::CalcLocalBounds() const {
        return FBox{ -mBoxExtent, mBoxExtent };
    }

    /**
     * @brief 상자의 로컬 축별 반길이를 설정합니다.
     */
    void Engine::UBoxComponent::SetBoxExtent(const FVector& InExtent) {
        mBoxExtent = FVector{ std::max(InExtent.x, 0.0f), std::max(InExtent.y, 0.0f), std::max(InExtent.z, 0.0f) };
    }

    /**
     * @brief 월드 스케일을 반영한 상자 축별 반길이를 반환합니다.
     */
    FVector Engine::UBoxComponent::GetScaledBoxExtent() const {
        const FVector Scale{GetTransform().mScale};

        return FVector{mBoxExtent.x * std::abs(Scale.x), mBoxExtent.y * std::abs(Scale.y), mBoxExtent.z * std::abs(Scale.z)};
    }

    /**
     * @brief 상자의 로컬 축별 반길이를 반환합니다.
     */
    const FVector& Engine::UBoxComponent::GetBoxExtent() const {
        return mBoxExtent;
    }

    /**
     * @brief 반길이를 전체 길이로 바꿔 설계도의 박스 모양을 다시 채웁니다.
     */
    void Engine::UBoxComponent::UpdateBodySetup() {
        constexpr float KindaSmallNumber{1.e-4f};
        constexpr float MinimumExtent{0.01f};

        // 크기가 0인 축은 아주 작은 크기로 바꿔 모양을 만들 수 있게 합니다.
        const FVector Extent{
            mBoxExtent.x < KindaSmallNumber ? MinimumExtent : mBoxExtent.x,
            mBoxExtent.y < KindaSmallNumber ? MinimumExtent : mBoxExtent.y,
            mBoxExtent.z < KindaSmallNumber ? MinimumExtent : mBoxExtent.z};

        mShapeBodySetup->mAggGeom = {};
        mShapeBodySetup->mAggGeom.mBoxElems.push_back(PhysicsRuntime::FKBoxElem{Extent.x * 2.0f, Extent.y * 2.0f, Extent.z * 2.0f});
    }

}
