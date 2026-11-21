/**
 * @file UBodySetup.cpp
 * @brief 하나의 바디를 이루는 충돌 형상을 보관하고 PhysX 형상으로 만듭니다.
 * @ingroup PhysicsRuntime
 */

#include "Core/Types.h"
#include "PhysicsRuntime/UBodySetup.h"
#include "PhysicsRuntime/PhysicsConversion.h"
#include "Core/Log/Log.h"

#include <PxPhysicsAPI.h>

#include <algorithm>

namespace PhysicsRuntime {

    /**
     * @brief 충돌 형상을 PhysX 형상으로 바꿀 때 쓰는 보조 함수와 로그 범주입니다.
     */
    namespace {
        DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

        /**
         * @brief 검증한 지오메트리와 로컬 자세로 강체 전용 형상을 만들어 붙입니다.
         * @param[in,out] Actor 형상을 붙일 PhysX 강체입니다.
         * @param[in] Geometry 스케일을 반영한 지오메트리입니다.
         * @param[in] LocalPose 바디 로컬 공간에서의 형상 자세입니다.
         * @param[in] Material 형상에 지정할 재질입니다.
         * @return 형상을 붙였으면 true, 지오메트리·자세가 유효하지 않거나 생성에 실패하면 false입니다.
         */
        bool AttachShape(physx::PxRigidActor& Actor, const physx::PxGeometry& Geometry, const physx::PxTransform& LocalPose, const physx::PxMaterial& Material) {
            if (!LocalPose.isValid()) {
                return false;
            }

            physx::PxShape* Shape{physx::PxRigidActorExt::createExclusiveShape(Actor, Geometry, Material)};

            if (Shape == nullptr) {
                return false;
            }

            Shape->setLocalPose(LocalPose);

            return true;
        }
    }

    /**
     * @brief 형상이 없는 바디 셋업을 구성합니다.
     */
    PhysicsRuntime::UBodySetup::UBodySetup() = default;

    /**
     * @brief 객체의 수명을 끝내고 소유한 리소스를 정리합니다.
     */
    PhysicsRuntime::UBodySetup::~UBodySetup() = default;

    /**
     * @brief 바디를 이루는 충돌 형상 묶음을 반환합니다.
     */
    PhysicsRuntime::FKAggregateGeom& PhysicsRuntime::UBodySetup::GetAggGeom() {
        return mAggGeom;
    }

    /**
     * @brief 바디를 이루는 충돌 형상 묶음을 반환합니다.
     */
    const PhysicsRuntime::FKAggregateGeom& PhysicsRuntime::UBodySetup::GetAggGeom() const {
        return mAggGeom;
    }

    /**
     * @brief 충돌 형상마다 PhysX 형상을 만들어 강체에 붙입니다.
     * @details 구는 가장 작은 축 스케일, 캡슐 반지름은 X·Y 중 큰 스케일, 캡슐 길이는 Z 스케일을 따릅니다.
     */
    Index_t PhysicsRuntime::UBodySetup::AddShapesToRigidActor(physx::PxRigidActor& Actor, const FVector& Scale, const physx::PxMaterial& Material) const {
        const FVector AbsoluteScale{Scale.Abs()};
        Index_t Attached{};

        for (Index_t Index{}; Index < mAggGeom.mSphereElems.size(); ++Index) {
            const PhysicsRuntime::FKSphereElem& Element{mAggGeom.mSphereElems[Index]};
            const physx::PxSphereGeometry Geometry{Element.mRadius * std::min({AbsoluteScale.x, AbsoluteScale.y, AbsoluteScale.z})};
            const physx::PxTransform LocalPose{PhysicsRuntime::ToPxVec3(Element.mCenter * Scale)};

            if (Geometry.isValid() && AttachShape(Actor, Geometry, LocalPose, Material)) {
                ++Attached;
            } else {
                MACAW_LOG(LogPhysics, Warning, "Skipped invalid sphere element {} of body setup '{}'.", Index, GetName());
            }
        }

        for (Index_t Index{}; Index < mAggGeom.mBoxElems.size(); ++Index) {
            const PhysicsRuntime::FKBoxElem& Element{mAggGeom.mBoxElems[Index]};
            const physx::PxBoxGeometry Geometry{PhysicsRuntime::ToPxVec3(Element.mHalfExtent * AbsoluteScale)};
            const physx::PxTransform LocalPose{PhysicsRuntime::ToPxVec3(Element.mCenter * Scale), PhysicsRuntime::ToPxQuat(Element.mRotation.Quaternion())};

            if (Geometry.isValid() && AttachShape(Actor, Geometry, LocalPose, Material)) {
                ++Attached;
            } else {
                MACAW_LOG(LogPhysics, Warning, "Skipped invalid box element {} of body setup '{}'.", Index, GetName());
            }
        }

        for (Index_t Index{}; Index < mAggGeom.mSphylElems.size(); ++Index) {
            const PhysicsRuntime::FKSphylElem& Element{mAggGeom.mSphylElems[Index]};
            const physx::PxCapsuleGeometry Geometry{Element.mRadius * std::max(AbsoluteScale.x, AbsoluteScale.y), Element.mLength * 0.5f * AbsoluteScale.z};
            const physx::PxTransform LocalPose{PhysicsRuntime::ToPxVec3(Element.mCenter * Scale), PhysicsRuntime::ToPxQuat(Element.mRotation.Quaternion()) * PhysicsRuntime::GetSphylBasis()};

            if (Geometry.isValid() && AttachShape(Actor, Geometry, LocalPose, Material)) {
                ++Attached;
            } else {
                MACAW_LOG(LogPhysics, Warning, "Skipped invalid capsule element {} of body setup '{}'.", Index, GetName());
            }
        }

        return Attached;
    }

}
