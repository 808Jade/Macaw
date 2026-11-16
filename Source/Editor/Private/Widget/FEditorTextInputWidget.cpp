#include "Editor/Widget/FEditorTextInputWidget.h"

#include <imgui_internal.h>
#include <algorithm>
#include <utility>

using namespace Editor;

FEditorTextInputWidget::FEditorTextInputWidget(Core::FString Id, Core::FString Label, Core::FString Hint, std::function<Core::FString()> Getter, std::function<void(const Core::FString&)> Setter, Index_t Capacity)
	: FEditorWidget{std::move(Id)},
	  mLabel{std::move(Label) + "###Value"},
	  mHint{std::move(Hint)},
	  mGetter{std::move(Getter)},
	  mSetter{std::move(Setter)} {
    mBuffer.resize(std::max<Index_t>(2, Capacity), '\0');

    if (mGetter) {
        SetText(mGetter());
    }
}

FEditorTextInputWidget::~FEditorTextInputWidget() = default;

std::string_view FEditorTextInputWidget::GetText() const {
    return {mBuffer.data()};
}

void FEditorTextInputWidget::SetText(std::string_view Text) {
    const Index_t Count{std::min(Text.size(), mBuffer.size() - 1)};
    std::copy_n(Text.data(), Count, mBuffer.data());
    mBuffer[Count] = '\0';
}

void FEditorTextInputWidget::SetCommittedHandler(std::function<void(const Core::FString&)> Handler) {
    mCommitted = std::move(Handler);
}

FVector2 FEditorTextInputWidget::ComputeDesiredSize() const {
    return {ImGui::CalcItemWidth() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x, ImGui::GetFrameHeight()};
}

void FEditorTextInputWidget::DrawContent(float) {
    if (mGetter && ImGui::GetActiveID() != ImGui::GetID(mLabel.c_str())) {
        const Core::FString Value{mGetter()};

        if (Value != GetText()) {
            SetText(Value);
        }
    }

    if (HasAllocatedSize()) {
        const float LabelWidth{ImGui::CalcTextSize(mLabel.c_str(), nullptr, true).x};
        ImGui::SetNextItemWidth(std::max(1.0f, GetAllocatedSize().x - (LabelWidth > 0.0f ? LabelWidth + ImGui::GetStyle().ItemInnerSpacing.x : 0.0f)));
    }

    mSubmitted = ImGui::InputTextWithHint(mLabel.c_str(), mHint.c_str(), mBuffer.data(), mBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    mChanged = ImGui::IsItemEdited();
}

void FEditorTextInputWidget::AfterDraw() {
    const Core::FString Value{GetText()};

    if (mChanged && mSetter) {
        mSetter(Value);
    }

    if (mSubmitted && mCommitted) {
        mCommitted(Value);
    }
}
