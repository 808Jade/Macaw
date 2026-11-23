/**
 * @file UShapeComponent.cpp
 * @brief 렌더 프록시 없이 단순 충돌 형상을 제공하고 에디터 보조 선으로 표시하는 프리미티브 컴포넌트입니다.
 * @ingroup Engine
 */

#include "Core/Types.h"
#include "Engine/Component/UShapeComponent.h"
#include "Engine/AActor.h"
#include "Engine/FPrimitiveDrawInterface.h"
#include "Engine/EPrimitiveType.h"
#include "Core/Math/FBox.h"

#include <algorithm>
#include <cmath>

namespace Engine {

    /**
     * @brief 현재 표시·에셋 상태에서 렌더 프록시를 생성할지 판정합니다.
     */
    bool Engine::UShapeComponent::ShouldCreateRenderState() const {
        return false;
    }

    /**
     * @brief 충돌 형상을 선택 강조 색상 또는 기본 색상의 선으로 수집합니다.
     */
    void Engine::UShapeComponent::DrawDebug(Engine::FPrimitiveDrawInterface& DrawInterface, bool Highlight) const {
        const FVector Color{Highlight ? FVector{1.0f, 0.0f, 0.0f} : FVector{mShapeColor.x, mShapeColor.y, mShapeColor.z}};

        DrawShape(DrawInterface, Color);
    }

    /**
     * @brief 선택한 경우에만 보조 충돌 도형을 그릴지 검사합니다.
     */
    bool Engine::UShapeComponent::ShouldDrawOnlyIfSelected() const {
        return mDrawOnlyIfSelected;
    }

    /**
     * @brief 충돌 형상에 적용할 최대 절대 축 스케일을 반환합니다.
     */
    float Engine::UShapeComponent::GetShapeScale() const {
        FVector Scale{GetTransform().mScale};

        return std::min({ std::abs(Scale.x), std::abs(Scale.y), std::abs(Scale.z) });
    }

    /**
     * @brief 컴포넌트 변환을 적용한 월드 충돌 경계를 계산합니다.
     */
    FBox Engine::UShapeComponent::CalcBounds() const {
        return CalcLocalBounds().Transformed(GetWorldMatrix());
    }

    /**
     * @brief 월드 공간의 축 정렬 경계 상자를 반환합니다.
     */
    FBox Engine::UShapeComponent::GetWorldBounds() const {
        return CalcBounds();
    }

    /**
     * @brief 표시할 프리미티브 메시 종류를 반환합니다.
     */
    Engine::EPrimitiveType Engine::UShapeComponent::GetPrimitiveType() const {
        return Engine::EPrimitiveType::Cube;
    }

    /**
     * @brief 충돌 도형을 표시할 RGBA 색상을 반환합니다.
     */
    const FVector4& Engine::UShapeComponent::GetShapeColor() const {
        return mShapeColor;
    }

    /**
     * @brief 충돌 도형을 표시할 RGBA 색상을 설정합니다.
     */
    bool Engine::UShapeComponent::SetShapeColor(const FVector4& Color) {
        if (!std::isfinite(Color.x) || !std::isfinite(Color.y) || !std::isfinite(Color.z) || !std::isfinite(Color.w)) {
            return false;
        }

        mShapeColor = Color;

        return true;
    }

    /**
     * @brief 선택한 경우에만 충돌 도형을 표시할지 여부를 설정합니다.
     */
    void Engine::UShapeComponent::SetDrawOnlyIfSelected(bool DrawOnlyIfSelected) {
        mDrawOnlyIfSelected = DrawOnlyIfSelected;
    }

}
