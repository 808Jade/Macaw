/**
 * @file FEditorDrawerManager.cpp
 * @brief 하단 드로어의 열기·닫기·애니메이션과 도킹 전환 및 설정을 관리합니다.
 * @ingroup Editor
 */

#include "FEditorDrawerManager.h"
#include "FEditorWorkspace.h"
#include "FEditorAnimation.h"
#include "EditorStyle.h"
#include "Serialization/JsonArchive.h"
#include "Editor/FEditorTabManager.h"
#include "Editor/FImGuiUIScale.h"
#include "Editor/Widget/FEditorWidget.h"
#include "Editor/Widget/FEditorContentWidget.h"
#include "Editor/Panel/FEditorPanel.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Editor;

FEditorDrawerManager::FEditorDrawerManager(FEditorTabManager& Tabs, FImGuiUIScale& UIScale, FEditorWorkspace& Workspace)
	: mTabs{Tabs},
	  mUIScale{UIScale},
	  mWorkspace{Workspace} {
}

FEditorDrawerManager::~FEditorDrawerManager() {
    for (FDrawer& Drawer : mDrawers) {
        Drawer.mTab->SetContent(nullptr);
        Drawer.mLayout->RemoveChild(*Drawer.mWidget);
        Drawer.mTab->SetContent(Drawer.mWidget);
    }
}

bool FEditorDrawerManager::RegisterDrawer(Core::FString Id, Core::FString Title, float Height) {
    FEditorWidget* Widget{mTabs.SpawnTab(Id)};
    FEditorTab* Tab{mTabs.FindTab(Id)};

    if (Widget == nullptr || Tab == nullptr || Tab->GetRole() != EEditorTabRole::Drawer || FindDrawer(Id) != nullptr) {
        return false;
    }

    auto Layout{std::make_unique<FEditorPanel>("DrawerLayout:" + Id)};
    auto Actions{std::make_unique<FEditorContentWidget>("DrawerActions", [this, Id](float) {
        DrawDockedActions(Id);
    }, []() {
        return FVector2{0.0f, ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y * 2.0f + std::max(1.0f, ImGui::GetStyle().SeparatorSize)};
    })};
    FEditorWidget* ActionsWidget{Actions.get()};
    ActionsWidget->SetVisible(false);
    Layout->AddChild(std::move(Actions), EEditorWidgetSizeRule::Auto);
    Tab->SetContent(nullptr);
    Widget->SetVisible(true);
    Layout->AddChild(*Widget);
    Tab->SetContent(Layout.get());
    mDrawers.push_back({std::move(Id), std::move(Title), Widget, Tab, std::move(Layout), ActionsWidget, Height});

    return true;
}

FEditorDrawerManager::FDrawer* FEditorDrawerManager::FindDrawer(const Core::FString& Id) {
    const auto Drawer{std::find_if(mDrawers.begin(), mDrawers.end(), [&Id](const FDrawer& Entry) {
        return Entry.mId == Id;
    })};

    return Drawer != mDrawers.end() ? &*Drawer : nullptr;
}

void FEditorDrawerManager::OpenDrawer(const Core::FString& Id) {
    if (FindDrawer(Id) == nullptr) {
        return;
    }

    if (IsDocked(Id)) {
        mWorkspace.FocusLevelEditor();
        mTabs.TryInvokeTab(Id);
        return;
    }

    for (FDrawer& Drawer : mDrawers) {
        if (Drawer.mId != Id && !IsDocked(Drawer.mId)) {
            Drawer.mTab->SetVisible(false);
        }
    }

    mTabs.TryInvokeTab(Id);
}

void FEditorDrawerManager::ToggleDrawer(const Core::FString& Id) {
    if (IsDocked(Id)) {
        OpenDrawer(Id);
    } else if (IsDrawerOpen(Id)) {
        mTabs.SetVisible(Id, false);
    } else {
        OpenDrawer(Id);
    }
}

void FEditorDrawerManager::CloseDrawer() {
    for (FDrawer& Drawer : mDrawers) {
        if (!IsDocked(Drawer.mId)) {
            Drawer.mTab->SetVisible(false);
        }
    }
}

bool FEditorDrawerManager::IsDrawerOpen(const Core::FString& Id) const {
    return !IsDocked(Id) && mTabs.IsVisible(Id);
}

bool FEditorDrawerManager::HasOpenDrawer() const {
    return std::any_of(mDrawers.begin(), mDrawers.end(), [](const FDrawer& Drawer) {
        return Drawer.mTab->GetRole() == EEditorTabRole::Drawer && Drawer.mTab->IsVisible();
    });
}

bool FEditorDrawerManager::IsDocked(const Core::FString& Id) const {
    const FEditorTab* Tab{mTabs.FindTab(Id)};
    return Tab != nullptr && Tab->GetRole() == EEditorTabRole::Panel;
}

void FEditorDrawerManager::DockDrawer(const Core::FString& Id) {
    if (FindDrawer(Id) != nullptr) {
        mPendingId = Id;
        mPendingDock = true;
    }
}

void FEditorDrawerManager::RestoreDrawer(const Core::FString& Id) {
    if (FindDrawer(Id) != nullptr) {
        mPendingId = Id;
        mPendingDock = false;
    }
}

void FEditorDrawerManager::SetChangedHandler(std::function<void()> Handler) {
    mChanged = std::move(Handler);
}

void FEditorDrawerManager::DrawDockedActions(const Core::FString& Id) {
    const float Width{ImGui::CalcTextSize("드로어로 전환").x + ImGui::GetStyle().FramePadding.x * 2.0f};
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - Width));

    if (EditorStyle::SmallButton("드로어로 전환")) {
        RestoreDrawer(Id);
    }

    ImGui::Separator();
}

void FEditorDrawerManager::Tick() {
    if (mContentDrawerCollapsedForDrag && ImGui::GetDragDropPayload() == nullptr) {
        mContentDrawerCollapsedForDrag = false;

        if (!IsDocked("Content")) {
            OpenDrawer("Content");
        }
    }

    FDrawer* Pending{FindDrawer(mPendingId)};
    if (Pending == nullptr) {
        return;
    }

    if (mPendingDock) {
        if (!mWorkspace.DockTab(mPendingId)) {
            return;
        }

        Pending->mTab->SetRole(EEditorTabRole::Panel);
        Pending->mActions->SetVisible(true);
        Pending->mTab->Open();
        if (mActiveId == mPendingId) {
            mActiveId.clear();
            mProgress = 0.0f;
            FEditorAnimation::ResetFloat(ImHashStr("EditorDrawerMotion"), "Open", 0.0f);
        }
    } else {
        Pending->mTab->SetRole(EEditorTabRole::Drawer);
        Pending->mActions->SetVisible(false);
        if (ImGuiWindow * Window{ImGui::FindWindowByName(Pending->mTab->GetName().c_str())}) {
            ImGui::DockContextProcessUndockWindow(ImGui::GetCurrentContext(), Window);
        }

        OpenDrawer(mPendingId);
    }

    mPendingId.clear();
    if (mChanged) {
        mChanged();
    }
}

void FEditorDrawerManager::SerializeSettings(Serialization::FJsonArchive& Archive) {
    if ((Archive.IsLoading() && !Archive.HasField("Drawers")) || !Archive.BeginObject("Drawers")) {
        return;
    }

    for (FDrawer& Drawer : mDrawers) {
        if (!Archive.BeginObject(Drawer.mId)) {
            continue;
        }

        bool Docked{IsDocked(Drawer.mId)};
        bool Visible{Docked && Drawer.mTab->IsVisible()};
        float Height{Drawer.mHeight};
        Archive.Serialize("Docked", Docked);
        Archive.Serialize("Visible", Visible);
        Archive.Serialize("Height", Height);
        Archive.EndObject();
        if (Archive.IsLoading() && !Archive.HasError()) {
            Drawer.mHeight = std::isfinite(Height) ? std::clamp(Height, 160.0f, 1200.0f) : 360.0f;
            Drawer.mTab->SetRole(Docked ? EEditorTabRole::Panel : EEditorTabRole::Drawer);
            Drawer.mActions->SetVisible(Docked);
            Drawer.mTab->SetVisible(Visible);
        }
    }

    Archive.EndObject();
}

void FEditorDrawerManager::Draw(float DeltaTime) {
    FDrawer* Requested{};

    for (FDrawer& Drawer : mDrawers) {
        if (IsDocked(Drawer.mId)) {
            continue;
        }

        const bool FocusRequested{Drawer.mTab->ConsumeFocusRequest()};

        if (Drawer.mTab->IsVisible() && (FocusRequested || (mActiveId.empty() && Requested == nullptr))) {
            Requested = &Drawer;
        }
    }

    const bool FocusRequested{Requested != nullptr};

    if (Requested != nullptr) {
        if (mActiveId.empty()) {
            const ImGuiWindow* Focused{ImGui::GetCurrentContext()->NavWindow};

            if (Focused != nullptr && Focused->RootWindow != ImGui::FindWindowByName("##EditorStatusBar")) {
                mPreviousFocus = Focused->RootWindow->Name;
            }
        }

        for (FDrawer& Drawer : mDrawers) {
            if (&Drawer != Requested && !IsDocked(Drawer.mId)) {
                Drawer.mTab->SetVisible(false);
            }
        }

        mActiveId = Requested->mId;
    }

    FDrawer* Active{FindDrawer(mActiveId)};

    if (Active == nullptr || IsDocked(mActiveId)) {
        const ImGuiWindow* Focused{ImGui::GetCurrentContext()->NavWindow};

        if (Focused != nullptr && Focused->RootWindow != ImGui::FindWindowByName("##EditorStatusBar")) {
            mPreviousFocus = Focused->RootWindow->Name;
        }

        return;
    }

    const bool Open{Active->mTab->IsVisible()};
    mProgress = std::clamp(FEditorAnimation::Float(ImHashStr("EditorDrawerMotion"), "Open", Open ? 1.0f : 0.0f, Open ? 0.20f : 0.14f), 0.0f, 1.0f);

    if (mProgress <= 0.0f) {
        mActiveId.clear();
        mPreviousFocus.clear();

        return;
    }

    const ImGuiViewport* Viewport{ImGui::GetMainViewport()};
    const float Scale{mUIScale.GetScale()};
    const float MaximumHeight{std::max(1.0f, Viewport->WorkSize.y * 0.8f)};
    const float Height{std::clamp(Active->mHeight * Scale, std::min(160.0f * Scale, MaximumHeight), MaximumHeight)};
    const float VisibleHeight{Height * mProgress};
    const float Margin{12.0f * Scale};
    const float Width{std::max(1.0f, Viewport->WorkSize.x - Margin * 2.0f)};
    const float Bottom{Viewport->WorkPos.y + Viewport->WorkSize.y - 4.0f * Scale};
    const ImVec2 Position{Viewport->WorkPos.x + Margin, Bottom - VisibleHeight};
    const bool PopupOpen{ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)};

    ImGui::SetNextWindowPos(Position);
    ImGui::SetNextWindowSize({Width, VisibleHeight});
    ImGui::SetNextWindowDockID(0, ImGuiCond_Always);

    if (FocusRequested) {
        ImGui::SetNextWindowFocus();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{1.0f, 1.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f * Scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0f * Scale, 6.0f * Scale});

    const ImGuiWindowFlags Flags{ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | (Open ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoInputs)};

    if (ImGui::Begin(Active->mTab->GetName().c_str(), nullptr, Flags)) {
        if (!PopupOpen) {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            ImDrawList* Shadow{ImGui::GetWindowDrawList()};
            Shadow->PushClipRect(Viewport->WorkPos, {Viewport->WorkPos.x + Viewport->WorkSize.x, Viewport->WorkPos.y + Viewport->WorkSize.y}, false);
            for (int Layer{6}; Layer > 0; --Layer) {
                const float Spread{static_cast<float>(Layer) * 1.5f * Scale};
                Shadow->AddRect({Position.x - Spread, Position.y - Spread}, {Position.x + Width + Spread, Bottom + Spread}, IM_COL32(0, 0, 0, 24), 6.0f * Scale + Spread, 0, 1.5f * Scale);
            }
            Shadow->PopClipRect();
        }

        ImGui::InvisibleButton("##ResizeDrawer", {std::max(1.0f, ImGui::GetContentRegionAvail().x), 5.0f * Scale});

        const float Center{Position.x + Width * 0.5f};
        ImGui::GetWindowDrawList()->AddRectFilled({Center - 20.0f * Scale, Position.y + 4.0f * Scale}, {Center + 20.0f * Scale, Position.y + 6.0f * Scale}, ImGui::GetColorU32(ImGuiCol_Separator), 2.0f * Scale);

        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        if (ImGui::IsItemDeactivated() && mChanged) {
            mChanged();
        }

        if (ImGui::IsItemActive() && mProgress >= 1.0f) {
            Active->mHeight = std::clamp(Height - ImGui::GetIO().MouseDelta.y, std::min(160.0f * Scale, MaximumHeight), MaximumHeight) / Scale;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(Active->mTitle.c_str());

        const float ActionsWidth{ImGui::CalcTextSize("레이아웃에 도킹").x + ImGui::CalcTextSize("닫기").x + ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x};
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), Width - ActionsWidth - ImGui::GetStyle().WindowPadding.x));

        if (EditorStyle::SmallButton("레이아웃에 도킹")) {
            DockDrawer(Active->mId);
        }

        ImGui::SameLine();

        if (EditorStyle::SmallButton("닫기")) {
            CloseDrawer();
        }

        ImGui::Separator();

        const float ContentHeight{std::max(1.0f, Height - ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y)};
        const bool Dragging{ImGui::GetDragDropPayload() != nullptr};

        if (ImGui::BeginChild("##DrawerContent", {0.0f, ContentHeight}, ImGuiChildFlags_None, ImGuiWindowFlags_NoSavedSettings)) {
            Active->mWidget->Draw(DeltaTime);
        }

        ImGui::EndChild();

        if (Open && Active->mId == "Content" && !Dragging && ImGui::GetDragDropPayload() != nullptr) {
            Active->mTab->SetVisible(false);
            mContentDrawerCollapsedForDrag = !Active->mTab->IsVisible();
        }

        const bool Focused{ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)};
        const ImVec2 Mouse{ImGui::GetIO().MousePos};
        const bool Outside{Mouse.y < Bottom && (Mouse.x < Position.x || Mouse.x >= Position.x + Width || Mouse.y < Position.y)};
        const bool ClickedOutside{Outside && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))};
        const ImGuiWindow* FocusedWindow{ImGui::GetCurrentContext()->NavWindow};
        const bool StatusBarFocused{FocusedWindow != nullptr && FocusedWindow->RootWindow == ImGui::FindWindowByName("##EditorStatusBar")};

        if (Open && !FocusRequested && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) && ImGui::GetDragDropPayload() == nullptr && (ClickedOutside || ImGui::IsKeyPressed(ImGuiKey_Escape, false) || (!Focused && !StatusBarFocused && FocusedWindow != nullptr))) {
            CloseDrawer();
        }

        if (!Active->mTab->IsVisible() && (Focused || StatusBarFocused) && !PopupOpen && !mPreviousFocus.empty()) {
            if (ImGuiWindow * Previous{ImGui::FindWindowByName(mPreviousFocus.c_str())}) {
                ImGui::FocusWindow(Previous);
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}

void FEditorDrawerManager::FinishDragFrame() {
    if (mContentDrawerCollapsedForDrag && ImGui::GetDragDropPayload() == nullptr) {
        mContentDrawerCollapsedForDrag = false;
    }
}
