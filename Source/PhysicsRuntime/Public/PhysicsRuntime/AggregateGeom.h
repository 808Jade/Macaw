/**
 * @file AggregateGeom.h
 * @brief 바디 하나를 이루는 충돌 모양 요소 목록을 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "PhysicsRuntime/BoxElem.h"
#include "PhysicsRuntime/SphereElem.h"
#include "PhysicsRuntime/SphylElem.h"

namespace PhysicsRuntime {
    /**
     * @brief 바디 하나를 이루는 구·박스·캡슐 충돌 모양 목록입니다.
     * @ingroup PhysicsRuntime
     */
    struct FKAggregateGeom {
    public:
        /**
         * @brief 모든 종류의 요소 개수를 합해 반환합니다.
         * @return 요소의 총 개수입니다.
         */
        Index_t GetElementCount() const {
            return mSphereElems.size() + mBoxElems.size() + mSphylElems.size();
        }

    public:
        /**
         * @brief 구 요소 목록입니다.
         */
        Core::TArray<FKSphereElem> mSphereElems{};

        /**
         * @brief 박스 요소 목록입니다.
         */
        Core::TArray<FKBoxElem> mBoxElems{};

        /**
         * @brief 캡슐 요소 목록입니다.
         */
        Core::TArray<FKSphylElem> mSphylElems{};
    };
}
