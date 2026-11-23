/**
 * @file FConstraintInstance.cpp
 * @brief 두 바디를 PhysX 관절로 잇고 움직임 한계를 적용합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FConstraintInstance.h"

#include "Core/Log/Log.h"
#include "Core/Math/FMatrix.h"
#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/PhysicsConversion.h"

#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogConstraint, Log, All);

    namespace {
        /**
         * @brief 0으로 둘 수 없는 한계 각도의 최솟값이며 단위는 도입니다.
         */
        constexpr float KindaSmallNumber{0.0001f};

        /**
         * @brief 한계 근처에서 미리 반응하기 시작하는 기본 각도 여유이며 단위는 도입니다.
         */
        constexpr float ContactDistanceDegrees{1.0f};

        /**
         * @brief 관절이 벌어졌을 때 강제로 붙이기 시작하는 거리이며 단위는 m입니다.
         */
        constexpr float ProjectionLinearTolerance{0.05f};

        /**
         * @brief 관절이 꺾였을 때 강제로 맞추기 시작하는 각도이며 단위는 도입니다.
         */
        constexpr float ProjectionAngularToleranceDegrees{180.0f};

        /**
         * @brief 도를 라디안으로 바꿉니다.
         * @param[in] Degrees 바꿀 각도입니다.
         * @return 라디안 값입니다.
         */
        float DegreesToRadians(float Degrees) {
            return Degrees * std::numbers::pi_v<float> / 180.0f;
        }

        /**
         * @brief 값을 최솟값과 최댓값 사이로 자릅니다.
         * @param[in] Value 자를 값입니다.
         * @param[in] Minimum 최솟값입니다.
         * @param[in] Maximum 최댓값입니다.
         * @return 자른 값이며, 최솟값이 최댓값보다 크면 최댓값입니다.
         */
        float Clamp(float Value, float Minimum, float Maximum) {
            return Value < Minimum ? Minimum : Value < Maximum ? Value : Maximum;
        }

        /**
         * @brief 회전 방식을 PhysX 값으로 바꿉니다.
         * @param[in] Motion 바꿀 회전 방식입니다.
         * @return PhysX 관절 축의 움직임 값입니다.
         */
        physx::PxD6Motion::Enum U2PAngularMotion(EAngularConstraintMotion Motion) {
            switch (Motion) {
                case EAngularConstraintMotion::Free:
                    return physx::PxD6Motion::eFREE;
                case EAngularConstraintMotion::Limited:
                    return physx::PxD6Motion::eLIMITED;
                case EAngularConstraintMotion::Locked:
                default:
                    return physx::PxD6Motion::eLOCKED;
            }
        }

        /**
         * @brief 이동 방식을 PhysX 값으로 바꿉니다.
         * @param[in] Motion 바꿀 이동 방식입니다.
         * @return PhysX 관절 축의 움직임 값입니다.
         * @note 한계 거리 값이 없으므로 Limited는 Locked로 다룹니다.
         */
        physx::PxD6Motion::Enum U2PLinearMotion(ELinearConstraintMotion Motion) {
            return Motion == ELinearConstraintMotion::Free ? physx::PxD6Motion::eFREE : physx::PxD6Motion::eLOCKED;
        }
    }

    /**
     * @brief PhysX 관절이 없는 기본 설정으로 구성합니다.
     */
    FConstraintInstance::FConstraintInstance() = default;

    /**
     * @brief PhysX 관절이 남아 있으면 해제합니다.
     */
    FConstraintInstance::~FConstraintInstance() {
        TermConstraint();
    }

    /**
     * @brief 다른 관절의 설정값만 복사합니다.
     */
    void FConstraintInstance::CopyConstraintParamsFrom(const FConstraintInstance& Other) {
        mJointName = Other.mJointName;
        mConstraintBone1 = Other.mConstraintBone1;
        mConstraintBone2 = Other.mConstraintBone2;
        mPos1 = Other.mPos1;
        mPriAxis1 = Other.mPriAxis1;
        mSecAxis1 = Other.mSecAxis1;
        mPos2 = Other.mPos2;
        mPriAxis2 = Other.mPriAxis2;
        mSecAxis2 = Other.mSecAxis2;
        mLinearXMotion = Other.mLinearXMotion;
        mLinearYMotion = Other.mLinearYMotion;
        mLinearZMotion = Other.mLinearZMotion;
        mSwing1Motion = Other.mSwing1Motion;
        mSwing2Motion = Other.mSwing2Motion;
        mTwistMotion = Other.mTwistMotion;
        mSwing1LimitDegrees = Other.mSwing1LimitDegrees;
        mSwing2LimitDegrees = Other.mSwing2LimitDegrees;
        mTwistLimitDegrees = Other.mTwistLimitDegrees;
    }

    /**
     * @brief 두 바디를 PhysX 관절로 잇고 움직임 한계를 적용합니다.
     */
    bool FConstraintInstance::InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, float Scale) {
        TermConstraint();

        physx::PxRigidActor* Actor1{Body1 != nullptr ? Body1->GetPxRigidActor() : nullptr};
        physx::PxRigidActor* Actor2{Body2 != nullptr ? Body2->GetPxRigidActor() : nullptr};

        if (Actor1 == nullptr && Actor2 == nullptr) {
            return false;
        }

        // 관절 위치는 바디의 스케일만큼 늘어난 자리에 둡니다.
        FTransform Local1{GetRefFrame(EConstraintFrame::Frame1)};
        FTransform Local2{GetRefFrame(EConstraintFrame::Frame2)};

        if (Actor1 != nullptr) {
            Local1.mLocation *= Scale;
        }

        if (Actor2 != nullptr) {
            Local2.mLocation *= Scale;
        }

        // 부모 바디를 앞에, 자식 바디를 뒤에 둡니다.
        mConstraintHandle = physx::PxD6JointCreate(PxGetPhysics(), Actor2, PhysicsRuntime::ToPxTransform(Local2), Actor1, PhysicsRuntime::ToPxTransform(Local1));

        if (mConstraintHandle == nullptr) {
            MACAW_LOG(LogConstraint, Warning, "Cannot create a PhysX D6 joint.");

            return false;
        }

        // 관절에서 이 객체를 찾을 수 있도록 주소를 넣습니다.
        mConstraintHandle->userData = this;

        // 이어진 두 바디끼리는 부딪히지 않고, 크게 벌어지면 강제로 붙입니다.
        mConstraintHandle->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
        mConstraintHandle->setConstraintFlag(physx::PxConstraintFlag::ePROJECTION, true);
        mConstraintHandle->setProjectionLinearTolerance(ProjectionLinearTolerance);
        mConstraintHandle->setProjectionAngularTolerance(DegreesToRadians(ProjectionAngularToleranceDegrees));

        mConstraintHandle->setMotion(physx::PxD6Axis::eX, U2PLinearMotion(mLinearXMotion));
        mConstraintHandle->setMotion(physx::PxD6Axis::eY, U2PLinearMotion(mLinearYMotion));
        mConstraintHandle->setMotion(physx::PxD6Axis::eZ, U2PLinearMotion(mLinearZMotion));

        // 두 흔들기 각도는 원뿔 하나로 함께 제한합니다.
        if (mSwing1Motion == EAngularConstraintMotion::Limited || mSwing2Motion == EAngularConstraintMotion::Limited) {
            const float Limit1Rad{DegreesToRadians(Clamp(mSwing1LimitDegrees, KindaSmallNumber, 179.9999f))};
            const float Limit2Rad{DegreesToRadians(Clamp(mSwing2LimitDegrees, KindaSmallNumber, 179.9999f))};
            const float ContactRad{DegreesToRadians(Clamp(ContactDistanceDegrees, 1.0f, std::min(mSwing1LimitDegrees, mSwing2LimitDegrees) * 0.49f))};

            mConstraintHandle->setSwingLimit(physx::PxJointLimitCone{Limit2Rad, Limit1Rad, ContactRad});
        }

        mConstraintHandle->setMotion(physx::PxD6Axis::eSWING2, U2PAngularMotion(mSwing1Motion));
        mConstraintHandle->setMotion(physx::PxD6Axis::eSWING1, U2PAngularMotion(mSwing2Motion));

        if (mTwistMotion == EAngularConstraintMotion::Limited) {
            const float TwistLimitRad{DegreesToRadians(mTwistLimitDegrees)};
            const float ContactRad{DegreesToRadians(Clamp(ContactDistanceDegrees, 1.0f, mTwistLimitDegrees * 0.95f))};

            mConstraintHandle->setTwistLimit(physx::PxJointAngularLimitPair{-TwistLimitRad, TwistLimitRad, ContactRad});
        }

        mConstraintHandle->setMotion(physx::PxD6Axis::eTWIST, U2PAngularMotion(mTwistMotion));

        return true;
    }

    /**
     * @brief PhysX 관절을 해제합니다.
     */
    void FConstraintInstance::TermConstraint() {
        if (mConstraintHandle == nullptr) {
            return;
        }

        // 해제 뒤 이 객체를 가리키지 않도록 주소를 먼저 지웁니다.
        mConstraintHandle->userData = nullptr;
        mConstraintHandle->release();
        mConstraintHandle = nullptr;
    }

    /**
     * @brief PhysX 관절이 만들어져 있는지 검사합니다.
     */
    bool FConstraintInstance::IsValidConstraintInstance() const {
        return mConstraintHandle != nullptr;
    }

    /**
     * @brief 한쪽 바디 기준의 관절 위치와 방향을 구합니다.
     */
    FTransform FConstraintInstance::GetRefFrame(EConstraintFrame Frame) const {
        const bool UseFrame1{Frame == EConstraintFrame::Frame1};
        const FVector& PriAxis{UseFrame1 ? mPriAxis1 : mPriAxis2};
        const FVector& SecAxis{UseFrame1 ? mSecAxis1 : mSecAxis2};
        const FVector& Position{UseFrame1 ? mPos1 : mPos2};
        const FVector Axes[]{PriAxis, SecAxis, PriAxis.Cross(SecAxis), Position};

        // 주 축, 보조 축, 두 축의 외적, 위치를 행으로 쌓아 변환을 만듭니다.
        FMatrix Matrix{FMatrix::Identity()};

        for (Index_t Row{}; Row < 4; ++Row) {
            Matrix(Row, 0) = Axes[Row].x;
            Matrix(Row, 1) = Axes[Row].y;
            Matrix(Row, 2) = Axes[Row].z;
        }

        FTransform Result{};

        if (!Matrix.TryGetTransform(Result) || std::abs(Matrix.Determinant() - 1.0f) > 0.01f) {
            MACAW_LOG(LogConstraint, Warning, "The constraint frame axes are not orthonormal.");
        }

        Result.mScale = FVector{1.0f, 1.0f, 1.0f};

        return Result;
    }
}
