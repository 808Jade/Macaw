/**
 * @file FKBoxElem.h
 * @brief 바디 셋업에 포함되는 상자 충돌 형상입니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/ReflectionMacros.h"
#include "Core/Math/FVector.h"
#include "Core/Math/FRotator.h"
#include "FKBoxElem.generated.h"

namespace PhysicsRuntime {

    /**
     * @brief 바디 셋업에 포함되는 상자 충돌 형상입니다.
     * @details 중심과 회전은 바디 로컬 공간 기준이며 반길이는 컴포넌트 스케일을 적용하기 전 값입니다.
     * @ingroup PhysicsRuntime
     */
    USTRUCT()
    struct FKBoxElem {
        GENERATED_BODY()

    public:

        /**
         * @brief 바디 로컬 공간에서의 상자 중심입니다.
         */
        UPROPERTY(Edit, Serialize)
        FVector mCenter{};

        /**
         * @brief 바디 로컬 공간에서의 상자 회전입니다.
         */
        UPROPERTY(Edit, Serialize)
        FRotator mRotation{};

        /**
         * @brief 스케일을 적용하기 전 상자의 축별 반길이(m)입니다.
         */
        UPROPERTY(Edit, Serialize)
        FVector mHalfExtent{0.5f, 0.5f, 0.5f};
    };

}
