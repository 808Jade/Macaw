#pragma once

#include "Editor/Widget/FEditorWidget.h"

namespace Editor {
    class FEditorSliderWidget final : public Editor::FEditorWidget {
    public:
        FEditorSliderWidget(Core::FString Id, Core::FString Label, float Minimum, float Maximum, std::function<float()> Getter, std::function<void(float)> Setter, Core::FString Format = "%.3f");
        ~FEditorSliderWidget() override;

    public:
        void SetDragSpeed(float Speed);
        FVector2 ComputeDesiredSize() const override;

    private:
        void DrawContent(float DeltaTime) override;
        void AfterDraw() override;

    private:
        Core::FString mLabel{};
        Core::FString mFormat{};
        std::function<float()> mGetter{};
        std::function<void(float)> mSetter{};
        float mMinimum{};
        float mMaximum{};
        float mDragSpeed{};
        float mEditedValue{};
        bool mChanged{};
    };
}
