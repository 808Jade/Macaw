/**
 * @file ALandscape.cpp
 * @brief 지형 액터의 구현입니다.
 * @ingroup Engine
 */

#include "Engine/ALandscape.h"
#include "Engine/Component/ULandscapeComponent.h"
#include "CoreUObject/Reflection/NewObject.h"

#include <utility>

namespace Engine {

    /**
     * @brief 컴포넌트가 없는 지형 액터를 구성합니다.
     */
    Engine::ALandscape::ALandscape() = default;

    /**
     * @brief 지형 액터를 정리합니다.
     */
    Engine::ALandscape::~ALandscape() = default;

    /**
     * @brief 기본 지형 컴포넌트를 추가합니다.
     */
    bool Engine::ALandscape::InitializeDefaultComponents() {
        if (!GetComponents().empty()) {
            return false;
        }

        auto Component{CoreUObject::NewObject<Engine::ULandscapeComponent>()};
        Component->SetName("LandscapeComponent");

        return AddComponent(std::move(Component));
    }

    /**
     * @brief 기본 지형 컴포넌트를 반환합니다.
     */
    Engine::ULandscapeComponent* Engine::ALandscape::GetLandscapeComponent() const {
        return static_cast<Engine::ULandscapeComponent*>(FindDefaultComponent(Engine::ULandscapeComponent::StaticClass(), Core::FName{"LandscapeComponent"}));
    }

}
