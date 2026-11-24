/**
 * @file UClothConfigNv.cpp
 * @brief NvCloth 천의 시뮬레이션 설정을 구현합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/UClothConfigNv.h"

namespace PhysicsRuntime {

    /**
     * @brief 기본 설정을 구성합니다.
     */
    UClothConfigNv::UClothConfigNv() = default;

    /**
     * @brief 설정을 정리합니다.
     */
    UClothConfigNv::~UClothConfigNv() = default;

    /**
     * @brief 천끼리 부딪히는 기능을 쓰는지 검사합니다.
     */
    bool UClothConfigNv::UseSelfCollisions() const {
        return mSelfCollisionRadius > 0.0f && mSelfCollisionStiffness > 0.0f;
    }

    /**
     * @brief 천끼리 부딪힐 입자를 고를 때 쓰는 거리를 구합니다.
     */
    float UClothConfigNv::NeedsSelfCollisionIndices() const {
        return UseSelfCollisions() ? mSelfCollisionRadius * mSelfCollisionCullScale : 0.0f;
    }
}
