#include "Editor/Widget/FEditorWidget.h"
#include "Editor/Panel/FEditorPanel.h"

#include <imgui.h>
#include <utility>

using namespace Editor;

FEditorWidget::FEditorWidget(Core::FString Id)
	: mId{std::move(Id)} {
}

FEditorWidget::~FEditorWidget() {
    mLifetime.reset();

    if (mParent != nullptr) {
        mParent->RemoveChild(*this);
    }
}

void FEditorWidget::Draw(float DeltaTime) {
    if (!IsVisible()) {
        return;
    }

    const bool Scoped{!UsesContentRegion()};

    if (Scoped) {
        ImGui::PushID(mId.c_str());
    }
    ImGui::BeginDisabled(!IsEnabled());
    DrawContent(DeltaTime);

    if (!mToolTip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", mToolTip.c_str());
    }

    ImGui::EndDisabled();

    if (Scoped) {
        ImGui::PopID();
    }

    AfterDraw();
}

void FEditorWidget::Draw(float DeltaTime, const FVector2& Size) {
    const std::weak_ptr<void> Lifetime{GetLifetime()};
    const FVector2 PreviousSize{mAllocatedSize};
    const bool HadSize{mHasAllocatedSize};
    mAllocatedSize = Size;
    mHasAllocatedSize = true;
    Draw(DeltaTime);

    if (!Lifetime.expired()) {
        mAllocatedSize = PreviousSize;
        mHasAllocatedSize = HadSize;
    }
}

FVector2 FEditorWidget::ComputeDesiredSize() const {
    return {};
}

bool FEditorWidget::UsesContentRegion() const {
    return false;
}

const FVector2& FEditorWidget::GetAllocatedSize() const {
    return mAllocatedSize;
}

bool FEditorWidget::HasAllocatedSize() const {
    return mHasAllocatedSize;
}

const Core::FString& FEditorWidget::GetId() const {
    return mId;
}

FEditorPanel* FEditorWidget::GetParent() const {
    return mParent;
}

bool FEditorWidget::IsVisible() const {
    return mVisible;
}

void FEditorWidget::SetVisible(bool Visible) {
    if (mVisible == Visible) {
        return;
    }

    mVisible = Visible;

    if (!mVisible) {
        NotifyHidden();
    }
}

bool FEditorWidget::IsEnabled() const {
    return mEnabled && (!mEnabledAttribute || mEnabledAttribute());
}

void FEditorWidget::SetEnabled(bool Enabled) {
    mEnabled = Enabled;
}

void FEditorWidget::SetEnabledAttribute(std::function<bool()> Attribute) {
    mEnabledAttribute = std::move(Attribute);
}

void FEditorWidget::SetToolTip(Core::FString ToolTip) {
    mToolTip = std::move(ToolTip);
}

bool FEditorWidget::AttachTo(const void* Owner) {
    if (Owner == nullptr || mOwner != nullptr) {
        return false;
    }

    mOwner = Owner;
    return true;
}

void FEditorWidget::DetachFrom(const void* Owner) {
    if (mOwner == Owner) {
        mOwner = nullptr;
    }
}

bool FEditorWidget::IsAttached() const {
    return mOwner != nullptr;
}

std::weak_ptr<void> FEditorWidget::GetLifetime() {
    if (mLifetime == nullptr) {
        mLifetime = std::make_shared<Uint8>();
    }

    return mLifetime;
}

void FEditorWidget::NotifyHidden() {
    OnHidden();
}

void FEditorWidget::AfterDraw() {
}

void FEditorWidget::OnHidden() {
}
