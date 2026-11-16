/**
 * @file FEditorToolkitHost.cpp
 * @brief 주 탭 내부의 도킹 공간과 위젯 탭·도구 모음·배치 생성을 관리합니다.
 * @ingroup Editor
 */

#include "Editor/FEditorToolkitHost.h"
#include "Editor/Widget/FEditorContentWidget.h"

#include <imgui_internal.h>
#include <utility>

using namespace Editor;

FEditorToolkitHost::FEditorToolkitHost(FEditorTab& Owner)
	: mOwner{Owner},
	  mTabs{&Owner} {
}

FEditorToolkitHost::~FEditorToolkitHost() = default;

FEditorTabManager& FEditorToolkitHost::GetTabManager() {
    return mTabs;
}

Uint32 FEditorToolkitHost::GetDockSpaceId() const {
    return mDockSpace;
}

bool FEditorToolkitHost::IsFocused() const {
    return mTabs.IsFocused();
}

void FEditorToolkitHost::SetDockSpaceName(Core::FString Name) {
    mDockSpaceName = std::move(Name);
}

void FEditorToolkitHost::SetFrameHandler(std::function<void()> Handler) {
    mFrameHandler = std::move(Handler);
}

void FEditorToolkitHost::SetToolbar(std::function<void()> Handler) {
    mToolbar = std::move(Handler);
}

void FEditorToolkitHost::SetLayoutBuilder(std::function<void(Uint32)> Handler) {
    mLayoutBuilder = std::move(Handler);
}

void FEditorToolkitHost::RequestLayoutReset() {
    mResetLayout = true;
}

void FEditorToolkitHost::DrawDockSpace(bool Visible) {
    mDockSpace = ImGui::GetID(mDockSpaceName.c_str());
    const ImGuiID ClassId{ImGui::GetCurrentWindow()->ID};
    mTabs.SetDockClass(ClassId);

    ImGuiWindowClass PanelClass{};
    PanelClass.ClassId = ClassId;
    PanelClass.DockingAllowUnclassed = false;

    constexpr ImGuiDockNodeFlags Flags{ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton};

    if (!Visible) {
        ImGui::DockSpace(mDockSpace, ImVec2{}, Flags | ImGuiDockNodeFlags_KeepAliveOnly, &PanelClass);

        return;
    }

    if (mFrameHandler) {
        mFrameHandler();
    }

    if (mToolbar) {
        mToolbar();
    }

    const ImGuiDockNode* Node{ImGui::DockBuilderGetNode(mDockSpace)};

    if (mResetLayout || Node == nullptr || (!Node->IsSplitNode() && Node->Windows.empty())) {
        ImGui::DockBuilderRemoveNode(mDockSpace);
        ImGui::DockBuilderAddNode(mDockSpace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(mDockSpace, ImGui::GetContentRegionAvail());

        if (mLayoutBuilder) {
            mLayoutBuilder(mDockSpace);
        } else {
            BuildDefaultLayout(mDockSpace);
        }

        ImGui::DockBuilderFinish(mDockSpace);
        mResetLayout = false;
    }

    ImGui::DockSpace(mDockSpace, ImVec2{}, Flags, &PanelClass);
}

void FEditorToolkitHost::DrawWindowMenu() {
    mTabs.DrawMenu();
}

void FEditorToolkitHost::AddWidget(Core::FString Id, Core::FString Label, std::unique_ptr<FEditorWidget> Widget, EEditorDockRegion Region) {
    const std::size_t Separator{mOwner.GetName().find("###")};
    const Core::FString OwnerId{Separator == Core::FString::npos ? mOwner.GetName() : mOwner.GetName().substr(Separator + 3)};
    const Core::FString Name{Label + "###" + OwnerId + ":" + Id};
    mTabs.RegisterWidget(std::move(Id), std::move(Label), std::move(Widget), Region, true, Name);
}

void FEditorToolkitHost::AddWidget(Core::FString Id, Core::FString Label, std::function<void(float)> Content, EEditorDockRegion Region, bool Closable, Core::FString WindowName) {
    const std::size_t Separator{mOwner.GetName().find("###")};
    const Core::FString OwnerId{Separator == Core::FString::npos ? mOwner.GetName() : mOwner.GetName().substr(Separator + 3)};
    const Core::FString Name{WindowName.empty() ? Label + "###" + OwnerId + ":" + Id : std::move(WindowName)};
    auto Widget{std::make_unique<FEditorContentWidget>(Id, std::move(Content))};
    mTabs.RegisterWidget(std::move(Id), std::move(Label), std::move(Widget), Region, Closable, Name);
}

void FEditorToolkitHost::BuildDefaultLayout(Uint32 DockSpace) {
    ImGuiID Center{DockSpace};
    ImGuiID Right{Center};
    ImGuiID Left{Center};
    ImGuiID Bottom{Center};
    ImGuiID Outliner{Center};

    if (mTabs.HasRegion(EEditorDockRegion::Details) || mTabs.HasRegion(EEditorDockRegion::Outliner)) {
        Right = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Right, 0.3f, nullptr, &Center);
        Outliner = Right;
    }

    if (mTabs.HasRegion(EEditorDockRegion::Left)) {
        Left = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Left, 0.25f, nullptr, &Center);
    }

    if (mTabs.HasRegion(EEditorDockRegion::Bottom)) {
        Bottom = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, 0.3f, nullptr, &Center);
    }

    if (mTabs.HasRegion(EEditorDockRegion::Details) && mTabs.HasRegion(EEditorDockRegion::Outliner)) {
        Right = ImGui::DockBuilderSplitNode(Right, ImGuiDir_Down, 0.55f, nullptr, &Outliner);
    }

    mTabs.ApplyLayout(Left, Outliner, Right, Bottom, Center);
}
