/**
 * @file PhysXPublicCore.cpp
 * @brief 엔진 변환과 PhysX 변환 사이의 변환 함수를 구현합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/PhysXPublicCore.h"

namespace PhysicsRuntime {
    /**
     * @brief 엔진 변환의 위치와 회전을 PhysX 변환으로 옮깁니다.
     */
    physx::PxTransform U2PTransform(const FTransform& Transform) {
        return physx::PxTransform{U2PVector(Transform.mLocation), U2PQuat(Transform.mRotation)};
    }

    /**
     * @brief PhysX 변환을 스케일 1인 엔진 변환으로 옮깁니다.
     */
    FTransform P2UTransform(const physx::PxTransform& Transform) {
        return FTransform{P2UVector(Transform.p), P2UQuat(Transform.q), FVector{1.0f, 1.0f, 1.0f}};
    }
}
