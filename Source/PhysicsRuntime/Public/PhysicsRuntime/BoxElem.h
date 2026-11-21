/**
 * @file BoxElem.h
 * @brief 박스 충돌 모양 요소를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FRotator.h"
#include "Core/Math/FTransform.h"
#include "PhysicsRuntime/ShapeElem.h"

namespace PhysicsRuntime {
    /**
     * @brief 바디 기준 위치·회전과 축별 전체 길이로 정의하는 박스 충돌 모양입니다.
     * @ingroup PhysicsRuntime
     */
    struct FKBoxElem : public FKShapeElem {
    public:
        /**
         * @brief 한 변이 1m인 박스를 구성합니다.
         */
        FKBoxElem()
            : FKShapeElem{EAggCollisionShape::Box} {}

        /**
         * @brief 축별 전체 길이로 박스를 구성합니다.
         * @param[in] X X축 전체 길이이며 단위는 m입니다.
         * @param[in] Y Y축 전체 길이이며 단위는 m입니다.
         * @param[in] Z Z축 전체 길이이며 단위는 m입니다.
         */
        FKBoxElem(float X, float Y, float Z)
            : FKShapeElem{EAggCollisionShape::Box}, mX{X}, mY{Y}, mZ{Z} {}

    public:
        /**
         * @brief 바디 기준 박스의 위치와 회전을 반환합니다.
         * @return 스케일이 1인 변환입니다.
         */
        FTransform GetTransform() const;

        /**
         * @brief 컴포넌트 스케일을 적용한 박스를 반환합니다.
         * @param[in] Scale3D 컴포넌트의 축별 스케일입니다.
         * @return 길이와 중심을 축별 스케일로 늘린 박스입니다.
         */
        FKBoxElem GetFinalScaled(const FVector& Scale3D) const;

    public:
        /**
         * @brief 바디 기준 박스의 중심이며 단위는 m입니다.
         */
        FVector mCenter{};

        /**
         * @brief 바디 기준 박스의 회전입니다.
         */
        FRotator mRotation{};

        /**
         * @brief X축 전체 길이이며 단위는 m입니다.
         */
        float mX{1.0f};

        /**
         * @brief Y축 전체 길이이며 단위는 m입니다.
         */
        float mY{1.0f};

        /**
         * @brief Z축 전체 길이이며 단위는 m입니다.
         */
        float mZ{1.0f};
    };
}
