/**
 * @file FConstraintInstance.h
 * @brief 두 뼈를 잇는 관절의 위치, 축과 움직임 한계를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FVector.h"
#include "Core/Types.h"

namespace PhysicsRuntime {
    /**
     * @brief 관절의 회전 축 하나가 움직일 수 있는 방식입니다.
     * @ingroup PhysicsRuntime
     */
    enum class EAngularConstraintMotion : Uint8 {
        /**
         * @brief 제한 없이 돌아갑니다.
         */
        Free,

        /**
         * @brief 한계 각도 안에서만 돌아갑니다.
         */
        Limited,

        /**
         * @brief 돌아가지 않습니다.
         */
        Locked
    };

    /**
     * @brief 관절의 이동 축 하나가 움직일 수 있는 방식입니다.
     * @ingroup PhysicsRuntime
     */
    enum class ELinearConstraintMotion : Uint8 {
        /**
         * @brief 제한 없이 움직입니다.
         */
        Free,

        /**
         * @brief 한계 거리 안에서만 움직입니다.
         */
        Limited,

        /**
         * @brief 움직이지 않습니다.
         */
        Locked
    };

    /**
     * @brief 두 뼈를 잇는 관절의 위치, 축과 움직임 한계입니다.
     * @ingroup PhysicsRuntime
     * @note Bone1은 자식 뼈, Bone2는 부모 뼈이며, 위치와 축은 각 뼈의 로컬 공간 기준입니다.
     */
    struct FConstraintInstance {
    public:
        /**
         * @brief 관절의 이름입니다.
         */
        Core::FString mJointName{};

        /**
         * @brief 관절이 잇는 첫 번째 뼈(자식)의 이름입니다.
         */
        Core::FString mConstraintBone1{};

        /**
         * @brief 관절이 잇는 두 번째 뼈(부모)의 이름입니다.
         */
        Core::FString mConstraintBone2{};

        /**
         * @brief 첫 번째 뼈 기준 관절 위치이며 단위는 m입니다.
         */
        FVector mPos1{};

        /**
         * @brief 첫 번째 뼈 기준 관절의 주 축(비틀기 축)입니다.
         */
        FVector mPriAxis1{1.0f, 0.0f, 0.0f};

        /**
         * @brief 첫 번째 뼈 기준 관절의 보조 축입니다.
         */
        FVector mSecAxis1{0.0f, 1.0f, 0.0f};

        /**
         * @brief 두 번째 뼈 기준 관절 위치이며 단위는 m입니다.
         */
        FVector mPos2{};

        /**
         * @brief 두 번째 뼈 기준 관절의 주 축(비틀기 축)입니다.
         */
        FVector mPriAxis2{1.0f, 0.0f, 0.0f};

        /**
         * @brief 두 번째 뼈 기준 관절의 보조 축입니다.
         */
        FVector mSecAxis2{0.0f, 1.0f, 0.0f};

        /**
         * @brief X축 이동 방식입니다.
         */
        ELinearConstraintMotion mLinearXMotion{ELinearConstraintMotion::Locked};

        /**
         * @brief Y축 이동 방식입니다.
         */
        ELinearConstraintMotion mLinearYMotion{ELinearConstraintMotion::Locked};

        /**
         * @brief Z축 이동 방식입니다.
         */
        ELinearConstraintMotion mLinearZMotion{ELinearConstraintMotion::Locked};

        /**
         * @brief 첫 번째 흔들기 축의 회전 방식입니다.
         */
        EAngularConstraintMotion mSwing1Motion{EAngularConstraintMotion::Free};

        /**
         * @brief 두 번째 흔들기 축의 회전 방식입니다.
         */
        EAngularConstraintMotion mSwing2Motion{EAngularConstraintMotion::Free};

        /**
         * @brief 비틀기 축의 회전 방식입니다.
         */
        EAngularConstraintMotion mTwistMotion{EAngularConstraintMotion::Free};

        /**
         * @brief 첫 번째 흔들기 축의 한계 각도이며 단위는 도입니다.
         */
        float mSwing1LimitDegrees{45.0f};

        /**
         * @brief 두 번째 흔들기 축의 한계 각도이며 단위는 도입니다.
         */
        float mSwing2LimitDegrees{45.0f};

        /**
         * @brief 비틀기 축의 한계 각도이며 단위는 도입니다.
         */
        float mTwistLimitDegrees{45.0f};
    };
}
