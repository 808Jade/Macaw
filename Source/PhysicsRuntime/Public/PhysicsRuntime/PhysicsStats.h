/**
 * @file PhysicsStats.h
 * @brief 물리 시뮬레이션 처리 시간과 바디·접촉 개수를 측정하는 통계 항목을 선언합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Stats.h"

namespace PhysicsRuntime {

    /**
     * @brief 물리 시뮬레이션 처리 시간과 바디·접촉 개수를 측정하는 통계 항목의 모음입니다.
     * @details 모두 physics 그룹에 속하므로 콘솔의 stat physics로 표시합니다.
     */
    namespace PhysicsStats {
        /**
         * @brief 장면 한 스텝의 simulate와 fetchResults 처리 시간을 측정하는 통계 항목입니다.
         */
        extern Core::FStat Simulate;

        /**
         * @brief 시뮬레이션 결과를 컴포넌트에 반영하고 충돌 이벤트를 전달하는 처리 시간을 측정하는 통계 항목입니다.
         */
        extern Core::FStat Dispatch;

        /**
         * @brief 진행한 고정 스텝 수를 세는 통계 항목입니다.
         */
        extern Core::FStat Steps;

        /**
         * @brief 스텝마다 자세가 바뀐 바디 수를 세는 통계 항목입니다.
         */
        extern Core::FStat MovedBodies;

        /**
         * @brief 새로 시작된 접촉 수를 세는 통계 항목입니다.
         */
        extern Core::FStat Contacts;

        /**
         * @brief 마지막으로 진행한 장면의 동적 바디 수입니다.
         */
        extern Core::FStat DynamicBodies;

        /**
         * @brief 마지막으로 진행한 장면의 정적 바디 수입니다.
         */
        extern Core::FStat StaticBodies;
    }

}
