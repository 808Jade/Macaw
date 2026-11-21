/**
 * @file UBodySetup.cpp
 * @brief 충돌 모양 요소에 스케일을 적용하고 PhysX 모양으로 만들어 액터에 붙입니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/UBodySetup.h"
#include "PhysicsRuntime/PhysXPublicCore.h"
#include "Core/Log/Log.h"

#include <PxPhysicsAPI.h>
#include <algorithm>
#include <cmath>

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

    namespace {
        /**
         * @brief 모양 크기의 최솟값이며 단위는 m입니다.
         */
        constexpr float KindaSmallNumber{1.e-4f};

        /**
         * @brief 캡슐 길이와 반지름의 최솟값이며 단위는 m입니다.
         */
        constexpr float MinimumSphylSize{0.001f};

        /**
         * @brief 모양 크기에 곱해 접촉 생성 거리를 구하는 비율입니다.
         */
        constexpr float ContactOffsetFactor{0.02f};

        /**
         * @brief 접촉 생성 거리의 최솟값이며 단위는 m입니다.
         */
        constexpr float MinContactOffset{0.02f};

        /**
         * @brief 접촉 생성 거리의 최댓값이며 단위는 m입니다.
         */
        constexpr float MaxContactOffset{0.08f};

        /**
         * @brief Z축 캡슐을 X축 PhysX 캡슐로 맞추기 위해 X와 Z를 맞바꾸는 회전입니다.
         * @return (X+Z) 방향 축으로 180도 돌리는 쿼터니언입니다.
         */
        physx::PxQuat GetSphylBasis() {
            const float InvSqrt2{1.0f / std::sqrt(2.0f)};

            return physx::PxQuat{physx::PxPi, physx::PxVec3{InvSqrt2, 0.0f, InvSqrt2}};
        }

        /**
         * @brief 모양의 가장 작은 크기로 접촉 생성 거리를 구합니다.
         * @param[in] MinSize 반지름이나 가장 짧은 반 길이이며 단위는 m입니다.
         * @return 최솟값과 최댓값 사이로 자른 접촉 생성 거리입니다.
         */
        float ComputeContactOffset(float MinSize) {
            return std::clamp(ContactOffsetFactor * MinSize, MinContactOffset, MaxContactOffset);
        }

        /**
         * @brief PhysX 모양 하나를 만들어 액터에 붙입니다.
         * @param[in] Actor 모양을 붙일 액터입니다.
         * @param[in] Geometry 모양의 형상입니다.
         * @param[in] LocalPose 액터 기준 모양의 위치와 회전입니다.
         * @param[in] Material 모양의 재질입니다.
         * @param[in] ContactSize 접촉 생성 거리를 구할 모양의 가장 작은 크기입니다.
         * @param[in] RestOffset 닿았다고 보는 표면 거리입니다.
         */
        void AttachShape(physx::PxRigidActor& Actor, const physx::PxGeometry& Geometry, const physx::PxTransform& LocalPose, physx::PxMaterial& Material, float ContactSize, float RestOffset) {
            const physx::PxShapeFlags Flags{physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE};

            physx::PxShape* Shape{PxGetPhysics().createShape(Geometry, Material, true, Flags)};

            if (Shape == nullptr) {
                MACAW_LOG(LogPhysics, Warning, "Cannot create a PhysX shape.");

                return;
            }

            Shape->setLocalPose(LocalPose);
            Shape->setContactOffset(ComputeContactOffset(ContactSize) + RestOffset);
            Shape->setRestOffset(RestOffset);

            // 액터가 참조를 가져가므로 만든 쪽의 참조는 바로 놓습니다.
            Actor.attachShape(*Shape);
            Shape->release();
        }
    }

    /**
     * @brief 컴포넌트 스케일을 적용한 구를 반환합니다.
     */
    FKSphereElem FKSphereElem::GetFinalScaled(const FVector& Scale3D) const {
        float MinScale{};
        float MinScaleAbs{};
        FVector Scale3DAbs{};

        SetupNonUniformHelper(Scale3D, MinScale, MinScaleAbs, Scale3DAbs);

        FKSphereElem ScaledSphere{*this};
        ScaledSphere.mRadius *= MinScaleAbs;
        ScaledSphere.mCenter = mCenter * Scale3D;

        return ScaledSphere;
    }

    /**
     * @brief 바디 기준 박스의 위치와 회전을 반환합니다.
     */
    FTransform FKBoxElem::GetTransform() const {
        return FTransform{mCenter, mRotation.Quaternion(), FVector{1.0f, 1.0f, 1.0f}};
    }

    /**
     * @brief 컴포넌트 스케일을 적용한 박스를 반환합니다.
     */
    FKBoxElem FKBoxElem::GetFinalScaled(const FVector& Scale3D) const {
        float MinScale{};
        float MinScaleAbs{};
        FVector Scale3DAbs{};

        SetupNonUniformHelper(Scale3D, MinScale, MinScaleAbs, Scale3DAbs);

        FKBoxElem ScaledBox{*this};
        ScaledBox.mX *= Scale3DAbs.x;
        ScaledBox.mY *= Scale3DAbs.y;
        ScaledBox.mZ *= Scale3DAbs.z;
        ScaledBox.mCenter = mCenter * Scale3D;

        return ScaledBox;
    }

    /**
     * @brief 컴포넌트 스케일을 적용한 반지름을 구합니다.
     */
    float FKSphylElem::GetScaledRadius(const FVector& Scale3D) const {
        const FVector Scale3DAbs{Scale3D.Abs()};
        const float RadiusScale{std::max(Scale3DAbs.x, Scale3DAbs.y)};

        return std::clamp(mRadius * RadiusScale, MinimumSphylSize, std::max(GetScaledHalfLength(Scale3DAbs), MinimumSphylSize));
    }

    /**
     * @brief 컴포넌트 스케일을 적용한 원기둥 부분의 길이를 구합니다.
     */
    float FKSphylElem::GetScaledCylinderLength(const FVector& Scale3D) const {
        return std::max(MinimumSphylSize, (GetScaledHalfLength(Scale3D) - GetScaledRadius(Scale3D)) * 2.0f);
    }

    /**
     * @brief 컴포넌트 스케일을 적용한 캡슐 전체 길이의 절반을 구합니다.
     */
    float FKSphylElem::GetScaledHalfLength(const FVector& Scale3D) const {
        return std::max((mLength + mRadius * 2.0f) * std::abs(Scale3D.z) * 0.5f, MinimumSphylSize);
    }

    /**
     * @brief 컴포넌트 스케일을 적용한 캡슐을 반환합니다.
     */
    FKSphylElem FKSphylElem::GetFinalScaled(const FVector& Scale3D) const {
        float MinScale{};
        float MinScaleAbs{};
        FVector Scale3DAbs{};

        SetupNonUniformHelper(Scale3D, MinScale, MinScaleAbs, Scale3DAbs);

        FKSphylElem ScaledSphyl{*this};
        ScaledSphyl.mRadius = GetScaledRadius(Scale3DAbs);
        ScaledSphyl.mLength = GetScaledCylinderLength(Scale3DAbs);
        ScaledSphyl.mCenter = mCenter * Scale3D;

        return ScaledSphyl;
    }

    /**
     * @brief 모양이 없는 빈 설계도를 구성합니다.
     */
    UBodySetup::UBodySetup() = default;

    /**
     * @brief 설계도를 정리합니다.
     */
    UBodySetup::~UBodySetup() = default;

    /**
     * @brief 모양 목록을 PhysX 모양으로 만들어 액터에 붙입니다.
     */
    void UBodySetup::AddShapesToRigidActor_AssumesLocked(physx::PxRigidActor& Actor, const FVector& Scale3D, physx::PxMaterial& Material) const {
        for (const FKSphereElem& Elem : mAggGeom.mSphereElems) {
            const FKSphereElem Scaled{Elem.GetFinalScaled(Scale3D)};
            const physx::PxSphereGeometry Geometry{std::max(Scaled.mRadius, KindaSmallNumber)};

            AttachShape(Actor, Geometry, physx::PxTransform{U2PVector(Scaled.mCenter)}, Material, Geometry.radius, Scaled.mRestOffset);
        }

        for (const FKSphylElem& Elem : mAggGeom.mSphylElems) {
            const FKSphylElem Scaled{Elem.GetFinalScaled(Scale3D)};
            const physx::PxCapsuleGeometry Geometry{std::max(Scaled.mRadius, KindaSmallNumber), std::max(Scaled.mLength * 0.5f, KindaSmallNumber)};

            // PhysX 캡슐은 X축 방향이므로 Z축 캡슐이 되도록 기준 회전을 곱합니다.
            const physx::PxTransform LocalPose{U2PVector(Scaled.mCenter), U2PQuat(Scaled.mRotation.Quaternion()) * GetSphylBasis()};

            AttachShape(Actor, Geometry, LocalPose, Material, Geometry.radius, Scaled.mRestOffset);
        }

        for (const FKBoxElem& Elem : mAggGeom.mBoxElems) {
            const FKBoxElem Scaled{Elem.GetFinalScaled(Scale3D)};

            // PhysX 박스는 전체 길이가 아니라 반 길이를 씁니다.
            const physx::PxBoxGeometry Geometry{std::max(Scaled.mX * 0.5f, KindaSmallNumber), std::max(Scaled.mY * 0.5f, KindaSmallNumber), std::max(Scaled.mZ * 0.5f, KindaSmallNumber)};

            AttachShape(Actor, Geometry, U2PTransform(Scaled.GetTransform()), Material, Geometry.halfExtents.minElement(), Scaled.mRestOffset);
        }
    }
}
