/**
 * @file FKSphereElem.h
 * @brief 바디 셋업에 포함되는 구 충돌 형상입니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/ReflectionMacros.h"
#include "Core/Math/FVector.h"
#include "FKSphereElem.generated.h"

namespace PhysicsRuntime {

    /**
     * @brief 바디 셋업에 포함되는 구 충돌 형상입니다.
     * @details 중심은 바디 로컬 공간 기준이며 반지름은 컴포넌트 스케일을 적용하기 전 값입니다.
     * @ingroup PhysicsRuntime
     */
    USTRUCT()
    struct FKSphereElem {
        GENERATED_BODY()

    public:

        /**
         * @brief 바디 로컬 공간에서의 구 중심입니다.
         */
        UPROPERTY(Edit, Serialize)
        FVector mCenter{};

        /**
         * @brief 스케일을 적용하기 전 구의 반지름(m)입니다.
         */
        UPROPERTY(Edit, Serialize)
        float mRadius{0.5f};
    };

}
