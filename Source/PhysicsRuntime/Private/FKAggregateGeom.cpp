/**
 * @file FKAggregateGeom.cpp
 * @brief 하나의 바디를 구성하는 단순 충돌 형상 묶음입니다.
 * @ingroup PhysicsRuntime
 */

#include "Core/Types.h"
#include "PhysicsRuntime/FKAggregateGeom.h"

namespace PhysicsRuntime {

    /**
     * @brief 포함한 모든 형상의 개수를 반환합니다.
     */
    Index_t PhysicsRuntime::FKAggregateGeom::GetElementCount() const {
        return mSphereElems.size() + mBoxElems.size() + mSphylElems.size();
    }

    /**
     * @brief 포함한 형상이 없는지 검사합니다.
     */
    bool PhysicsRuntime::FKAggregateGeom::IsEmpty() const {
        return GetElementCount() == 0;
    }

}
