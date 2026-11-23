/**
 * @file UBodySetup.h
 * @brief 하나의 바디를 이루는 충돌 형상을 보관하고 PhysX 형상으로 만듭니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "Core/Math/FVector.h"
#include "PhysicsRuntime/UBodySetupCore.h"
#include "PhysicsRuntime/FKAggregateGeom.h"
#include "UBodySetup.generated.h"

namespace physx {
    class PxRigidActor;
}

namespace PhysicsRuntime {
    class FPhysicsRuntime;
}

namespace PhysicsRuntime {

    /**
     * @brief 하나의 바디를 이루는 충돌 형상을 보관하고 PhysX 형상으로 만듭니다.
     * @details 스태틱 메시는 바디 셋업 하나를, 피직스 에셋은 본마다 바디 셋업 하나를 가집니다.
     *          형상 크기는 스케일을 적용하기 전 값이며, 바디를 만들 때 컴포넌트 스케일을 형상 크기에 반영합니다.
     * @ingroup PhysicsRuntime
     */
    UCLASS()
    class UBodySetup : public PhysicsRuntime::UBodySetupCore {
        GENERATED_BODY()

    public:

        /**
         * @brief 형상이 없는 바디 셋업을 구성합니다.
         */
        UBodySetup();

        /**
         * @brief 객체의 수명을 끝내고 소유한 리소스를 정리합니다.
         */
        ~UBodySetup() override;

    public:

        /**
         * @brief 바디를 이루는 충돌 형상 묶음을 반환합니다.
         * @return 이 객체가 보관하는 값의 참조입니다.
         */
        PhysicsRuntime::FKAggregateGeom& GetAggGeom();

        /**
         * @brief 바디를 이루는 충돌 형상 묶음을 반환합니다.
         * @return 이 객체가 보관하는 값의 참조입니다.
         */
        const PhysicsRuntime::FKAggregateGeom& GetAggGeom() const;

        /**
         * @brief 충돌 형상마다 PhysX 형상을 만들어 강체에 붙입니다.
         * @param[in,out] Actor 형상을 붙일 PhysX 강체입니다.
         * @param[in] Scale 형상 크기와 중심에 반영할 컴포넌트 스케일입니다.
         * @param[in] Runtime 형상에 지정할 기본 재질과 볼록 메시 쿠킹을 제공하는 PhysX 런타임입니다.
         * @return 강체에 붙인 형상의 개수입니다.
         * @note 스케일을 반영한 크기가 유효하지 않은 형상은 건너뜁니다. 회전한 상자·캡슐에 비균등 스케일을 주면 근사합니다.
         *       볼록 형상은 호출할 때마다 쿠킹하며, 쿠킹한 메시는 형상만 참조하므로 바디가 해제될 때 함께 해제됩니다.
         */
        Index_t AddShapesToRigidActor(physx::PxRigidActor& Actor, const FVector& Scale, const PhysicsRuntime::FPhysicsRuntime& Runtime) const;

    private:

        /**
         * @brief 바디를 이루는 충돌 형상 묶음입니다.
         */
        UPROPERTY(Serialize)
        PhysicsRuntime::FKAggregateGeom mAggGeom{};
    };

}
