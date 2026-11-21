/**
 * @file PhysXPublicCore.h
 * @brief 엔진 수학 타입과 PhysX 수학 타입 사이의 변환 함수를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FQuat.h"
#include "Core/Math/FTransform.h"
#include "Core/Math/FVector.h"
#include <PxPhysicsAPI.h>

namespace PhysicsRuntime {
    /**
     * @brief 엔진 벡터를 PhysX 벡터로 변환합니다.
     * @param[in] Vector 변환할 엔진 벡터입니다.
     * @return 같은 성분의 PhysX 벡터입니다.
     */
    inline physx::PxVec3 U2PVector(const FVector& Vector) {
        return physx::PxVec3{Vector.x, Vector.y, Vector.z};
    }

    /**
     * @brief 엔진 쿼터니언을 PhysX 쿼터니언으로 변환합니다.
     * @param[in] Quat 변환할 엔진 쿼터니언입니다.
     * @return 같은 (x, y, z, w) 성분의 PhysX 쿼터니언입니다.
     */
    inline physx::PxQuat U2PQuat(const FQuat& Quat) {
        return physx::PxQuat{Quat.x, Quat.y, Quat.z, Quat.w};
    }

    /**
     * @brief 엔진 변환의 위치와 회전을 PhysX 변환으로 옮깁니다.
     * @param[in] Transform 변환할 엔진 변환입니다.
     * @return 위치와 회전만 담은 PhysX 변환입니다.
     * @note PhysX 변환에는 스케일이 없으므로 스케일은 버립니다.
     */
    physx::PxTransform U2PTransform(const FTransform& Transform);

    /**
     * @brief PhysX 벡터를 엔진 벡터로 변환합니다.
     * @param[in] Vector 변환할 PhysX 벡터입니다.
     * @return 같은 성분의 엔진 벡터입니다.
     */
    inline FVector P2UVector(const physx::PxVec3& Vector) {
        return FVector{Vector.x, Vector.y, Vector.z};
    }

    /**
     * @brief PhysX 쿼터니언을 엔진 쿼터니언으로 변환합니다.
     * @param[in] Quat 변환할 PhysX 쿼터니언입니다.
     * @return 같은 (x, y, z, w) 성분의 엔진 쿼터니언입니다.
     */
    inline FQuat P2UQuat(const physx::PxQuat& Quat) {
        return FQuat{Quat.x, Quat.y, Quat.z, Quat.w};
    }

    /**
     * @brief PhysX 변환을 엔진 변환으로 옮깁니다.
     * @param[in] Transform 변환할 PhysX 변환입니다.
     * @return 위치와 회전을 담고 스케일이 1인 엔진 변환입니다.
     */
    FTransform P2UTransform(const physx::PxTransform& Transform);
}
