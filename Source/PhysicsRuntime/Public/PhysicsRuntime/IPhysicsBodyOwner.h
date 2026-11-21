/**
 * @file IPhysicsBodyOwner.h
 * @brief 바디가 주인에게 물리 결과를 알리는 인터페이스를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FTransform.h"

namespace PhysicsRuntime {
    /**
     * @brief 바디를 소유한 객체가 구현하는 인터페이스입니다.
     * @ingroup PhysicsRuntime
     * @note 물리 모듈이 바디 주인의 구체 타입을 몰라도 연락할 수 있게 합니다.
     */
    class IPhysicsBodyOwner {
    public:
        /**
         * @brief 인터페이스를 통해 주인을 정리할 수 있게 합니다.
         */
        virtual ~IPhysicsBodyOwner() = default;

        /**
         * @brief 시뮬레이션으로 바뀐 바디의 위치와 회전을 받습니다.
         * @param[in] NewTransform 바디의 새 월드 변환이며 스케일은 1입니다.
         */
        virtual void OnPhysicsTransformUpdated(const FTransform& NewTransform) = 0;
    };
}
