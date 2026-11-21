/**
 * @file UBodySetup.h
 * @brief 바디 하나의 충돌 모양 목록을 보관하고 PhysX 모양으로 만드는 설계도를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "PhysicsRuntime/AggregateGeom.h"
#include "PhysicsRuntime/UBodySetupCore.h"
#include "UBodySetup.generated.h"

namespace physx {
    class PxMaterial;
    class PxRigidActor;
}

namespace PhysicsRuntime {
    /**
     * @brief 바디 하나의 충돌 모양 목록을 보관하고 PhysX 모양으로 만드는 설계도입니다.
     * @ingroup PhysicsRuntime
     * @note 같은 설계도를 여러 바디가 공유하며, 바디마다 다른 스케일을 적용합니다.
     */
    UCLASS()
    class UBodySetup : public PhysicsRuntime::UBodySetupCore {
        GENERATED_BODY()

    public:
        /**
         * @brief 모양이 없는 빈 설계도를 구성합니다.
         */
        UBodySetup();

        /**
         * @brief 설계도를 정리합니다.
         */
        ~UBodySetup() override;

    public:
        /**
         * @brief 모양 목록을 PhysX 모양으로 만들어 액터에 붙입니다.
         * @param[in] Actor 모양을 붙일 PhysX 액터입니다.
         * @param[in] Scale3D 바디의 축별 스케일입니다.
         * @param[in] Material 모양에 적용할 PhysX 재질입니다.
         * @note 호출하는 쪽이 장면 쓰기 권한을 가지고 있어야 합니다.
         */
        void AddShapesToRigidActor_AssumesLocked(physx::PxRigidActor& Actor, const FVector& Scale3D, physx::PxMaterial& Material) const;

    public:
        /**
         * @brief 바디를 이루는 구·박스·캡슐 모양 목록입니다.
         */
        FKAggregateGeom mAggGeom{};
    };
}
