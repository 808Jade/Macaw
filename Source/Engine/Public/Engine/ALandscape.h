/**
 * @file ALandscape.h
 * @brief 하이트맵 지형을 배치하는 액터입니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/ReflectionMacros.h"
#include "Engine/AActor.h"
#include "ALandscape.generated.h"

namespace Engine {

    class ULandscapeComponent;

    /**
     * @brief 지형 컴포넌트를 기본으로 갖는 액터입니다.
     * @ingroup Engine
     */
    UCLASS()
    class ALandscape final : public Engine::AActor {
        GENERATED_BODY()

    public:
        /**
         * @brief 컴포넌트가 없는 지형 액터를 구성합니다.
         */
        ALandscape();

        /**
         * @brief 지형 액터를 정리합니다.
         */
        ~ALandscape() override;

    public:
        /**
         * @brief 기본 지형 컴포넌트를 추가합니다.
         * @return 컴포넌트가 없던 액터에 추가하면 true입니다.
         */
        bool InitializeDefaultComponents() override;

        /**
         * @brief 기본 지형 컴포넌트를 반환합니다.
         * @return 액터가 소유한 컴포넌트의 비소유 주소이며, 없으면 nullptr입니다.
         */
        Engine::ULandscapeComponent* GetLandscapeComponent() const;
    };

}
