#include "Editor/Widget/FEditorCheckBoxWidget.h"

#include <imgui.h>
#include <utility>

using namespace Editor;

FEditorCheckBoxWidget::FEditorCheckBoxWidget(Core::FString Id, Core::FString Label, std::function<bool()> Getter, std::function<void(bool)> Setter)
	: FEditorWidget{std::move(Id)},
	  mLabel{std::move(Label) + "###Value"},
	  mGetter{std::move(Getter)},
	  mSetter{std::move(Setter)} {
}

FEditorCheckBoxWidget::~FEditorCheckBoxWidget() = default;

FVector2 FEditorCheckBoxWidget::ComputeDesiredSize() const {
    return {ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x, ImGui::GetFrameHeight()};
}

void FEditorCheckBoxWidget::DrawContent(float) {
    mChanged = false;

    if (mGetter && mSetter) {
        mEditedValue = mGetter();
        mChanged = ImGui::Checkbox(mLabel.c_str(), &mEditedValue);
    }
}

void FEditorCheckBoxWidget::AfterDraw() {
    if (mChanged) {
        mSetter(mEditedValue);
    }
}
