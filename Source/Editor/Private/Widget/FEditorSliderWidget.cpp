#include "Editor/Widget/FEditorSliderWidget.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Editor;

FEditorSliderWidget::FEditorSliderWidget(Core::FString Id, Core::FString Label, float Minimum, float Maximum, std::function<float()> Getter, std::function<void(float)> Setter, Core::FString Format)
	: FEditorWidget{std::move(Id)},
	  mLabel{std::move(Label) + "###Value"},
	  mFormat{std::move(Format)},
	  mGetter{std::move(Getter)},
	  mSetter{std::move(Setter)},
	  mMinimum{Minimum},
	  mMaximum{std::max(Minimum, Maximum)} {
}

FEditorSliderWidget::~FEditorSliderWidget() = default;

void FEditorSliderWidget::SetDragSpeed(float Speed) {
    mDragSpeed = std::max(0.0f, Speed);
}

FVector2 FEditorSliderWidget::ComputeDesiredSize() const {
    return {ImGui::CalcItemWidth() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x, ImGui::GetFrameHeight()};
}

void FEditorSliderWidget::DrawContent(float) {
    mChanged = false;

    if (!mGetter || !mSetter) {
        return;
    }

    mEditedValue = mGetter();

    if (HasAllocatedSize()) {
        const float LabelWidth{ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x};
        ImGui::SetNextItemWidth(std::max(1.0f, GetAllocatedSize().x - (LabelWidth > 0.0f ? LabelWidth + ImGui::GetStyle().ItemInnerSpacing.x : 0.0f)));
    }

    mChanged = mDragSpeed > 0.0f ? ImGui::DragFloat(mLabel.c_str(), &mEditedValue, mDragSpeed, mMinimum, mMaximum, mFormat.c_str(), ImGuiSliderFlags_AlwaysClamp) : ImGui::SliderFloat(mLabel.c_str(), &mEditedValue, mMinimum, mMaximum, mFormat.c_str(), ImGuiSliderFlags_AlwaysClamp);
}

void FEditorSliderWidget::AfterDraw() {
    if (mChanged && std::isfinite(mEditedValue)) {
        mSetter(std::clamp(mEditedValue, mMinimum, mMaximum));
    }
}
