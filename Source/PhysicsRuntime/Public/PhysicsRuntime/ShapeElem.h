/**
 * @file ShapeElem.h
 * @brief 충돌 모양 요소의 공통 부모와 스케일 보조 함수를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FVector.h"
#include "Core/Types.h"

#include <algorithm>
#include <cmath>

namespace PhysicsRuntime {
    /**
     * @brief 충돌 모양 요소의 종류입니다.
     * @ingroup PhysicsRuntime
     */
    enum class EAggCollisionShape : Uint8 {
        /**
         * @brief 구입니다.
         */
        Sphere,

        /**
         * @brief 박스입니다.
         */
        Box,

        /**
         * @brief 캡슐입니다.
         */
        Sphyl,

        /**
         * @brief 종류가 정해지지 않았습니다.
         */
        Unknown
    };

    /**
     * @brief 모든 충돌 모양 요소의 공통 부모입니다.
     * @ingroup PhysicsRuntime
     */
    struct FKShapeElem {
    public:
        /**
         * @brief 종류가 정해지지 않은 요소를 구성합니다.
         */
        FKShapeElem() = default;

        /**
         * @brief 지정한 종류의 요소를 구성합니다.
         * @param[in] ShapeType 요소의 종류입니다.
         */
        explicit FKShapeElem(EAggCollisionShape ShapeType)
            : mShapeType{ShapeType} {}

    public:
        /**
         * @brief 요소의 종류를 반환합니다.
         * @return 요소의 종류입니다.
         */
        EAggCollisionShape GetShapeType() const {
            return mShapeType;
        }

    public:
        /**
         * @brief 다른 물체와 닿았다고 보는 표면까지의 거리이며 단위는 m입니다.
         */
        float mRestOffset{};

    private:
        /**
         * @brief 요소의 종류입니다.
         */
        EAggCollisionShape mShapeType{EAggCollisionShape::Unknown};
    };

    /**
     * @brief 스케일의 절댓값과 가장 작은 축 배율을 구합니다.
     * @param[in] InScale3D 컴포넌트의 축별 스케일입니다.
     * @param[out] OutMinScale 부호를 반영한 가장 작은 축 배율입니다. 세 축이 모두 음수일 때만 음수입니다.
     * @param[out] OutMinScaleAbs 가장 작은 축 배율의 절댓값입니다.
     * @param[out] OutScale3DAbs 축별 스케일의 절댓값입니다.
     * @note 스케일이 0에 가까우면 0.1로 바꿔 크기가 0인 모양을 만들지 않습니다.
     */
    inline void SetupNonUniformHelper(FVector InScale3D, float& OutMinScale, float& OutMinScaleAbs, FVector& OutScale3DAbs) {
        constexpr float NearlyZero{1.e-8f};
        constexpr float MinimumScale{0.1f};

        if (std::abs(InScale3D.x) <= NearlyZero && std::abs(InScale3D.y) <= NearlyZero && std::abs(InScale3D.z) <= NearlyZero) {
            InScale3D = FVector{MinimumScale, MinimumScale, MinimumScale};
        }

        OutScale3DAbs = InScale3D.Abs();
        OutMinScaleAbs = std::min({OutScale3DAbs.x, OutScale3DAbs.y, OutScale3DAbs.z});
        OutMinScale = std::max({InScale3D.x, InScale3D.y, InScale3D.z}) < 0.0f ? -OutMinScaleAbs : OutMinScaleAbs;

        // 한 축만 0인 경우에도 최소 배율을 보장합니다.
        if (std::abs(OutMinScale) <= NearlyZero) {
            OutMinScale = MinimumScale;
            OutMinScaleAbs = MinimumScale;
        }
    }
}
