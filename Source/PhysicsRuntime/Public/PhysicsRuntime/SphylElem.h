/**
 * @file SphylElem.h
 * @brief Z축으로 선 캡슐 충돌 모양 요소를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FRotator.h"
#include "Core/Math/FTransform.h"
#include "PhysicsRuntime/ShapeElem.h"

namespace PhysicsRuntime {
    /**
     * @brief 바디 기준 위치·회전, 반지름과 원기둥 길이로 정의하는 캡슐 충돌 모양입니다.
     * @ingroup PhysicsRuntime
     * @note 회전이 없으면 캡슐 축은 Z축입니다.
     */
    struct FKSphylElem : public FKShapeElem {
    public:
        /**
         * @brief 반지름과 원기둥 길이가 1m인 캡슐을 구성합니다.
         */
        FKSphylElem()
            : FKShapeElem{EAggCollisionShape::Sphyl} {}

        /**
         * @brief 지정한 반지름과 원기둥 길이로 캡슐을 구성합니다.
         * @param[in] Radius 캡슐의 반지름이며 단위는 m입니다.
         * @param[in] Length 양 끝 반구를 뺀 원기둥 부분의 길이이며 단위는 m입니다.
         */
        FKSphylElem(float Radius, float Length)
            : FKShapeElem{EAggCollisionShape::Sphyl}, mRadius{Radius}, mLength{Length} {}

    public:
        /**
         * @brief 컴포넌트 스케일을 적용한 반지름을 구합니다.
         * @param[in] Scale3D 컴포넌트의 축별 스케일입니다.
         * @return X·Y 중 큰 배율을 적용하고 캡슐 길이의 절반을 넘지 않게 자른 반지름입니다.
         */
        float GetScaledRadius(const FVector& Scale3D) const;

        /**
         * @brief 컴포넌트 스케일을 적용한 원기둥 부분의 길이를 구합니다.
         * @param[in] Scale3D 컴포넌트의 축별 스케일입니다.
         * @return 전체 길이에서 양 끝 반구를 뺀 길이입니다.
         */
        float GetScaledCylinderLength(const FVector& Scale3D) const;

        /**
         * @brief 컴포넌트 스케일을 적용한 캡슐 전체 길이의 절반을 구합니다.
         * @param[in] Scale3D 컴포넌트의 축별 스케일입니다.
         * @return 반구를 포함한 전체 길이에 Z 배율을 적용한 값의 절반입니다.
         */
        float GetScaledHalfLength(const FVector& Scale3D) const;

        /**
         * @brief 컴포넌트 스케일을 적용한 캡슐을 반환합니다.
         * @param[in] Scale3D 컴포넌트의 축별 스케일입니다.
         * @return 반지름, 원기둥 길이와 중심에 스케일을 적용한 캡슐입니다.
         */
        FKSphylElem GetFinalScaled(const FVector& Scale3D) const;

    public:
        /**
         * @brief 바디 기준 캡슐의 중심이며 단위는 m입니다.
         */
        FVector mCenter{};

        /**
         * @brief 바디 기준 캡슐의 회전입니다.
         */
        FRotator mRotation{};

        /**
         * @brief 캡슐의 반지름이며 단위는 m입니다.
         */
        float mRadius{1.0f};

        /**
         * @brief 양 끝 반구를 뺀 원기둥 부분의 길이이며 단위는 m입니다.
         */
        float mLength{1.0f};
    };
}
