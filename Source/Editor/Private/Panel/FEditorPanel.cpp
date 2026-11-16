/**
 * @file FEditorPanel.cpp
 * @brief 자식 위젯을 수평·수직·겹침 방식으로 배치하는 복합 위젯입니다.
 * @ingroup Editor
 */

#include "Editor/Panel/FEditorPanel.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Editor;

FEditorPanel::FEditorPanel(Core::FString Id, EEditorPanelLayout Layout)
	: FEditorWidget{std::move(Id)},
	  mLayout{Layout} {
}

FEditorPanel::~FEditorPanel() {
    ClearChildren();
}

bool FEditorPanel::CanAddChild(const FEditorWidget& Widget) const {
    if (Widget.IsAttached()) {
        return false;
    }

    for (const FEditorWidget* Ancestor{this}; Ancestor != nullptr; Ancestor = Ancestor->mParent) {
        if (Ancestor == &Widget) {
            return false;
        }
    }

    return true;
}

Index_t FEditorPanel::AddChild(FEditorWidget& Widget, EEditorWidgetSizeRule SizeRule, float Extent, const FVector4& Padding) {
    if (!CanAddChild(Widget) || !std::isfinite(Extent) || !std::isfinite(Padding.x) || !std::isfinite(Padding.y) || !std::isfinite(Padding.z) || !std::isfinite(Padding.w)) {
        return InvalidIndex;
    }

    FSlot Slot{};
    Slot.mWidget = &Widget;
    mChildren.push_back(std::move(Slot));
    Widget.mParent = this;
    Widget.AttachTo(this);
    const Index_t Index{mChildren.size() - 1};
    SetChildLayout(Index, SizeRule, Extent, Padding);

    return Index;
}

Index_t FEditorPanel::AddChild(std::unique_ptr<FEditorWidget> Widget, EEditorWidgetSizeRule SizeRule, float Extent, const FVector4& Padding) {
    if (Widget == nullptr || !CanAddChild(*Widget)) {
        return InvalidIndex;
    }

    const Index_t Index{AddChild(*Widget, SizeRule, Extent, Padding)};

    if (Index != InvalidIndex) {
        mChildren[Index].mOwnedWidget = std::move(Widget);
    }

    return Index;
}

bool FEditorPanel::RemoveChild(FEditorWidget& Widget) {
    const auto Entry{std::find_if(mChildren.begin(), mChildren.end(), [&Widget](const FSlot& Slot) {
        return Slot.mWidget == &Widget;
    })};

    if (Entry == mChildren.end()) {
        return false;
    }

    Entry->mWidget->mParent = nullptr;
    Entry->mWidget->DetachFrom(this);
    mChildren.erase(Entry);

    return true;
}

void FEditorPanel::ClearChildren() {
    for (FSlot& Slot : mChildren) {
        Slot.mWidget->mParent = nullptr;
        Slot.mWidget->DetachFrom(this);
    }

    mChildren.clear();
}

Index_t FEditorPanel::GetChildCount() const {
    return mChildren.size();
}

FEditorWidget* FEditorPanel::GetChild(Index_t Index) const {
    return Index < mChildren.size() ? mChildren[Index].mWidget : nullptr;
}

bool FEditorPanel::SetChildLayout(Index_t Index, EEditorWidgetSizeRule SizeRule, float Extent, const FVector4& Padding) {
    if (Index >= mChildren.size() || !std::isfinite(Extent) || !std::isfinite(Padding.x) || !std::isfinite(Padding.y) || !std::isfinite(Padding.z) || !std::isfinite(Padding.w)) {
        return false;
    }

    FSlot& Slot{mChildren[Index]};
    Slot.mSizeRule = SizeRule;
    Slot.mExtent = std::max(0.0f, Extent);
    Slot.mPadding = {std::max(0.0f, Padding.x), std::max(0.0f, Padding.y), std::max(0.0f, Padding.z), std::max(0.0f, Padding.w)};

    return true;
}

void FEditorPanel::SetLayout(EEditorPanelLayout Layout) {
    mLayout = Layout;
}

EEditorPanelLayout FEditorPanel::GetLayout() const {
    return mLayout;
}

void FEditorPanel::SetSpacing(float Spacing) {
    if (std::isfinite(Spacing)) {
        mSpacing = std::max(0.0f, Spacing);
    }
}

float FEditorPanel::GetSpacing() const {
    return mSpacing;
}

Core::TArray<FEditorPanel::FArrangement> FEditorPanel::ArrangeChildren(const FVector2& Size) const {
    Core::TArray<FArrangement> Arrangements{};
    const FVector2 Available{std::max(0.0f, Size.x), std::max(0.0f, Size.y)};
    const bool Horizontal{mLayout == EEditorPanelLayout::Horizontal};
    float Reserved{};
    float FillWeight{};
    Index_t VisibleCount{};

    for (const FSlot& Slot : mChildren) {
        if (!Slot.mWidget->IsVisible()) {
            continue;
        }

        ++VisibleCount;
        const FVector2 Desired{Slot.mWidget->ComputeDesiredSize()};
        Reserved += Horizontal ? Slot.mPadding.x + Slot.mPadding.z : Slot.mPadding.y + Slot.mPadding.w;

        if (Slot.mSizeRule == EEditorWidgetSizeRule::Fill) {
            FillWeight += Slot.mExtent;
        } else {
            Reserved += Slot.mSizeRule == EEditorWidgetSizeRule::Fixed ? Slot.mExtent : std::max(0.0f, Horizontal ? Desired.x : Desired.y);
        }
    }

    const float Spacing{VisibleCount > 1 ? mSpacing * static_cast<float>(VisibleCount - 1) : 0.0f};
    const float Remaining{std::max(0.0f, (Horizontal ? Available.x : Available.y) - Reserved - Spacing)};
    float Offset{};

    for (const FSlot& Slot : mChildren) {
        if (!Slot.mWidget->IsVisible()) {
            continue;
        }

        const FVector2 Desired{Slot.mWidget->ComputeDesiredSize()};
        float Extent{Slot.mSizeRule == EEditorWidgetSizeRule::Fixed ? Slot.mExtent : std::max(0.0f, Horizontal ? Desired.x : Desired.y)};

        if (Slot.mSizeRule == EEditorWidgetSizeRule::Fill) {
            Extent = FillWeight > 0.0f ? Remaining * Slot.mExtent / FillWeight : 0.0f;
        }

        FArrangement Arrangement{};
        Arrangement.mWidget = Slot.mWidget;
        Arrangement.mPosition = {std::min(Available.x, Slot.mPadding.x), std::min(Available.y, Slot.mPadding.y)};
        Arrangement.mSize = {std::max(0.0f, Available.x - Slot.mPadding.x - Slot.mPadding.z), std::max(0.0f, Available.y - Slot.mPadding.y - Slot.mPadding.w)};

        if (mLayout != EEditorPanelLayout::Overlay) {
            if (Horizontal) {
                Arrangement.mPosition.x = std::min(Available.x, Slot.mPadding.x + Offset);
                Arrangement.mSize.x = std::min(Extent, std::max(0.0f, Available.x - Arrangement.mPosition.x - Slot.mPadding.z));
                Offset += Slot.mPadding.x + Extent + Slot.mPadding.z + mSpacing;
            } else {
                Arrangement.mPosition.y = std::min(Available.y, Slot.mPadding.y + Offset);
                Arrangement.mSize.y = std::min(Extent, std::max(0.0f, Available.y - Arrangement.mPosition.y - Slot.mPadding.w));
                Offset += Slot.mPadding.y + Extent + Slot.mPadding.w + mSpacing;
            }
        }

        Arrangements.push_back(Arrangement);
    }

    return Arrangements;
}

FVector2 FEditorPanel::ComputeDesiredSize() const {
    FVector2 Desired{};
    Index_t Count{};

    for (const FSlot& Slot : mChildren) {
        if (!Slot.mWidget->IsVisible()) {
            continue;
        }

        FVector2 Child{Slot.mWidget->ComputeDesiredSize()};

        if (Slot.mSizeRule == EEditorWidgetSizeRule::Fixed && mLayout != EEditorPanelLayout::Overlay) {
            if (mLayout == EEditorPanelLayout::Horizontal) {
                Child.x = Slot.mExtent;
            } else {
                Child.y = Slot.mExtent;
            }
        }

        Child.x = std::max(0.0f, Child.x) + Slot.mPadding.x + Slot.mPadding.z;
        Child.y = std::max(0.0f, Child.y) + Slot.mPadding.y + Slot.mPadding.w;

        if (mLayout == EEditorPanelLayout::Horizontal) {
            Desired.x += Child.x;
            Desired.y = std::max(Desired.y, Child.y);
        } else if (mLayout == EEditorPanelLayout::Vertical) {
            Desired.x = std::max(Desired.x, Child.x);
            Desired.y += Child.y;
        } else {
            Desired.x = std::max(Desired.x, Child.x);
            Desired.y = std::max(Desired.y, Child.y);
        }

        ++Count;
    }

    if (Count > 1 && mLayout != EEditorPanelLayout::Overlay) {
        if (mLayout == EEditorPanelLayout::Horizontal) {
            Desired.x += mSpacing * static_cast<float>(Count - 1);
        } else {
            Desired.y += mSpacing * static_cast<float>(Count - 1);
        }
    }

    return Desired;
}

void FEditorPanel::DrawContent(float DeltaTime) {
    const ImVec2 Origin{ImGui::GetCursorScreenPos()};
    const ImVec2 Available{HasAllocatedSize() ? ImVec2{GetAllocatedSize().x, GetAllocatedSize().y} : ImGui::GetContentRegionAvail()};
    const Core::TArray<FArrangement> Arrangements{ArrangeChildren({Available.x, Available.y})};

    for (const FArrangement& Arrangement : Arrangements) {
        const bool Registered{std::any_of(mChildren.begin(), mChildren.end(), [&Arrangement](const FSlot& Slot) {
            return Slot.mWidget == Arrangement.mWidget;
        })};

        if (!Registered || Arrangement.mSize.x < 1.0f || Arrangement.mSize.y < 1.0f) {
            continue;
        }

        const ImVec2 Position{Origin.x + Arrangement.mPosition.x, Origin.y + Arrangement.mPosition.y};
        const ImVec2 Maximum{Position.x + Arrangement.mSize.x, Position.y + Arrangement.mSize.y};
        ImGui::SetCursorScreenPos(Position);

        if (Arrangement.mWidget->UsesContentRegion()) {
            ImGui::PushID(Arrangement.mWidget->GetId().c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
            const bool Visible{ImGui::BeginChild("ContentRegion", {Arrangement.mSize.x, Arrangement.mSize.y}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings)};
            ImGui::PopStyleVar();

            if (Visible) {
                Arrangement.mWidget->Draw(DeltaTime, Arrangement.mSize);
            }

            ImGui::EndChild();
            ImGui::PopID();
        } else {
            ImGui::PushClipRect(Position, Maximum, true);
            Arrangement.mWidget->Draw(DeltaTime, Arrangement.mSize);
            ImGui::PopClipRect();
        }
    }

    ImGui::SetCursorScreenPos(Origin);
    ImGui::Dummy({std::max(0.0f, Available.x), std::max(0.0f, Available.y)});
}

void FEditorPanel::OnHidden() {
    Core::TArray<FEditorWidget*> Children{};

    for (const FSlot& Slot : mChildren) {
        Children.push_back(Slot.mWidget);
    }

    for (FEditorWidget* Widget : Children) {
        if (std::any_of(mChildren.begin(), mChildren.end(), [Widget](const FSlot& Slot) {
                return Slot.mWidget == Widget;
            })) {
            Widget->NotifyHidden();
        }
    }
}
