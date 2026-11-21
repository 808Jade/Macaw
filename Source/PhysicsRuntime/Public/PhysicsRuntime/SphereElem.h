/**
 * @file SphereElem.h
 * @brief 구 충돌 모양 요소를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "PhysicsRuntime/ShapeElem.h"

namespace PhysicsRuntime {
    /**
     * @brief 바디 기준 위치와 반지름으로 정의하는 구 충돌 모양입니다.
     * @ingroup PhysicsRuntime
     */
    struct FKSphereElem : public FKShapeElem {
    public:
        /**
         * @brief 반지름 1m인 구를 구성합니다.
         */
        FKSphereElem()
            : FKShapeElem{EAggCollisionShape::Sphere} {}

        /**
         * @brief 지정한 반지름의 구를 구성합니다.
         * @param[in] Radius 구의 반지름이며 단위는 m입니다.
         */
        explicit FKSphereElem(float Radius)
            : FKShapeElem{EAggCollisionShape::Sphere}, mRadius{Radius} {}

    public:
        /**
         * @brief 컴포넌트 스케일을 적용한 구를 반환합니다.
         * @param[in] Scale3D 컴포넌트의 축별 스케일입니다.
         * @return 반지름은 가장 작은 축 배율로, 중심은 축별 스케일로 늘린 구입니다.
         */
        FKSphereElem GetFinalScaled(const FVector& Scale3D) const;

    public:
        /**
         * @brief 바디 기준 구의 중심이며 단위는 m입니다.
         */
        FVector mCenter{};

        /**
         * @brief 구의 반지름이며 단위는 m입니다.
         */
        float mRadius{1.0f};
    };
}
