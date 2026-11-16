#include "Editor/Widget/FEditorButtonWidget.h"
#include "EditorStyle.h"

#include <imgui.h>
#include <utility>

using namespace Editor;

FEditorButtonWidget::FEditorButtonWidget(Core::FString Id, Core::FString Label, std::function<void()> OnClicked)
	: FEditorWidget{std::move(Id)},
	  mLabel{std::move(Label)},
	  mOnClicked{std::move(OnClicked)} {
}

FEditorButtonWidget::~FEditorButtonWidget() = default;

void FEditorButtonWidget::SetLabel(Core::FString Label) {
    mLabel = std::move(Label);
}

void FEditorButtonWidget::SetIcon(EEditorIcon Icon) {
    mIcon = Icon;
}

void FEditorButtonWidget::SetStyle(EEditorButtonStyle Style) {
    mStyle = Style;
}

void FEditorButtonWidget::SetDropdown(bool Dropdown) {
    mDropdown = Dropdown;
}

void FEditorButtonWidget::SetSelected(bool Selected) {
    mSelected = Selected;
}

void FEditorButtonWidget::SetSize(const FVector2& Size) {
    mSize = Size;
}

void FEditorButtonWidget::SetClickedHandler(std::function<void()> Handler) {
    mOnClicked = std::move(Handler);
}

bool FEditorButtonWidget::WasClicked() const {
    return mClicked;
}

FVector2 FEditorButtonWidget::ComputeDesiredSize() const {
    const ImVec2 Text{ImGui::CalcTextSize(mLabel.c_str())};
    const ImGuiStyle& Style{ImGui::GetStyle()};
    const float Height{mStyle == EEditorButtonStyle::Small ? ImGui::GetTextLineHeight() : ImGui::GetFrameHeight()};
    const float Width{mStyle == EEditorButtonStyle::Icon ? Height : Text.x + Style.FramePadding.x * 2.0f + (mIcon.has_value() ? Height + Style.ItemInnerSpacing.x : 0.0f) + (mDropdown ? 12.0f * Style.FontScaleMain * Style.FontScaleDpi : 0.0f)};
    return {mSize.x > 0.0f ? mSize.x : Width, mSize.y > 0.0f ? mSize.y : Height};
}

void FEditorButtonWidget::DrawContent(float) {
    const FVector2 Size{HasAllocatedSize() ? GetAllocatedSize() : mSize};

    if (mStyle == EEditorButtonStyle::Icon && mIcon.has_value()) {
        mClicked = EditorStyle::IconButton("##Button", *mIcon, mSelected, IsEnabled(), mDropdown, nullptr, Size.x);
    } else if (mStyle == EEditorButtonStyle::Label) {
        mClicked = EditorStyle::LabelButton(mLabel.c_str(), mIcon.has_value() ? &*mIcon : nullptr, mDropdown);
    } else {
        const Core::FString Label{mLabel + "###Button"};
        mClicked = mStyle == EEditorButtonStyle::Small ? EditorStyle::SmallButton(Label.c_str()) : EditorStyle::Button(Label.c_str(), {Size.x, Size.y}, mSelected);
    }
}

void FEditorButtonWidget::AfterDraw() {
    if (mClicked && mOnClicked) {
        mOnClicked();
    }
}
