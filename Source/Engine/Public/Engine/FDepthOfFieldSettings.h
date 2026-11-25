/**
 * @file FDepthOfFieldSettings.h
 * @brief 카메라별 피사계 심도 설정입니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/Types.h"
#include "Core/Literals/QuantityTypes.h"
#include "CoreUObject/Reflection/TNativeEnum.h"
#include "FDepthOfFieldSettings.generated.h"

namespace Engine {

    UENUM()
    enum class EDepthOfFieldApertureShape : Uint8 {
        Circle,
        Pentagon,
        Hexagon
    };

    USTRUCT()
    struct FDepthOfFieldSettings {
        GENERATED_BODY()

    public:
        UPROPERTY(Edit, Serialize)
        bool mEnabled{false};

        /** @brief 카메라에서 초점 구간 중심까지의 거리(m)입니다. */
        UPROPERTY(Edit, Serialize)
        Core::FLength mFocalDistance{Core::FLength::FromBase(5.0f)};

        /** @brief 선명한 초점 구간의 전체 폭(m)입니다. */
        UPROPERTY(Edit, Serialize)
        Core::FLength mFocusWidth{Core::FLength::FromBase(2.0f)};

        /** @brief 초점 구간의 각 경계에서 최대 흐림까지의 거리(m)입니다. */
        UPROPERTY(Edit, Serialize)
        Core::FLength mTransitionDistance{Core::FLength::FromBase(2.0f)};

        /** @brief 전체 해상도에서의 블러 반경 상한(px)입니다. */
        UPROPERTY(Edit, Serialize)
        float mMaxBlurRadius{12.0f};

        UPROPERTY(Edit, Serialize)
        float mFStop{2.8f};

        UPROPERTY(Edit, Serialize)
        Engine::EDepthOfFieldApertureShape mApertureShape{Engine::EDepthOfFieldApertureShape::Circle};

        UPROPERTY(Edit, Serialize)
        bool mNearBlur{true};

        UPROPERTY(Edit, Serialize)
        bool mFarBlur{true};
    };

}
