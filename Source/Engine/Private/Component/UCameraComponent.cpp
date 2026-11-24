/**
 * @file UCameraComponent.cpp
 * @brief 시야각 설정과 월드 변환으로부터 카메라 뷰 생성에 대한 구현입니다.
 * @ingroup Engine
 */

#include "Engine/Component/UCameraComponent.h"
#include "Core/Literals/AngleFunctions.h"
#include "Engine/FMinimalViewInfo.h"
#include "Engine/FSceneView.h"
#include <cmath>

using namespace Core::Literals;

namespace Engine {

    Engine::UCameraComponent::UCameraComponent()
		: mFieldOfView{60_deg} {
    }

    /**
     * @brief 카메라의 수직 시야각을 반환합니다.
     */
    Core::FAngle Engine::UCameraComponent::GetFieldOfView() const {
        return mFieldOfView;
    }

    /**
     * @brief 유효한 범위의 시야각을 검사하여 카메라에 설정합니다.
     */
    bool Engine::UCameraComponent::SetFieldOfView(Core::FAngle FieldOfView) {
        if (!FieldOfView.IsFinite() || FieldOfView <= 0_deg || FieldOfView >= 180_deg) {
            return false;
        }

        mFieldOfView = FieldOfView;

        return true;
    }

    const Engine::FDepthOfFieldSettings& Engine::UCameraComponent::GetDepthOfFieldSettings() const {
        return mDepthOfFieldSettings;
    }

    bool Engine::UCameraComponent::SetDepthOfFieldSettings(const Engine::FDepthOfFieldSettings& Settings) {
        if (!std::isfinite(Settings.mFocalDistance) || Settings.mFocalDistance < 0.0f ||
            !std::isfinite(Settings.mFocusWidth) || Settings.mFocusWidth < 0.0f ||
            !std::isfinite(Settings.mTransitionDistance) || Settings.mTransitionDistance < 0.0f ||
            !std::isfinite(Settings.mMaxBlurRadius) || Settings.mMaxBlurRadius < 0.0f ||
            !std::isfinite(Settings.mFStop) || Settings.mFStop <= 0.0f) {
            return false;
        }

        if (Settings.mApertureShape != Engine::EDepthOfFieldApertureShape::Circle &&
            Settings.mApertureShape != Engine::EDepthOfFieldApertureShape::Pentagon &&
            Settings.mApertureShape != Engine::EDepthOfFieldApertureShape::Hexagon) {
            return false;
        }

        mDepthOfFieldSettings = Settings;

        return true;
    }

    bool Engine::UCameraComponent::SetDepthOfFieldFocalDistance(float Distance) {
        Engine::FDepthOfFieldSettings Settings{mDepthOfFieldSettings};
        Settings.mFocalDistance = Distance;

        return SetDepthOfFieldSettings(Settings);
    }

    Engine::FMinimalViewInfo Engine::UCameraComponent::GetCameraViewInfo() const {
        Engine::FMinimalViewInfo ViewInfo{};
        ViewInfo.mLocation = GetTransform().mLocation;
        ViewInfo.mRotation = GetTransform().mRotation;
        ViewInfo.mFieldOfView = mFieldOfView;
        ViewInfo.mDepthOfFieldSettings = mDepthOfFieldSettings;

        return ViewInfo;
    }

    /**
     * @brief 카메라의 월드 변환과 시야각으로 뷰·투영 행렬을 계산합니다.
     */
    bool Engine::UCameraComponent::GetCameraView(const FVector2& Size, Engine::FSceneView& View) const {
        return GetTransform().IsValid() && GetCameraViewInfo().CalcSceneView(Size, View);
    }

}
