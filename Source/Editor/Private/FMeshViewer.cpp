/**
 * @file FMeshViewer.cpp
 * @brief 독립 메시 미리보기와 궤도 카메라의 구현입니다.
 * @ingroup Editor
 */

#include "Core/Types.h"
#include "Editor/FMeshViewer.h"
#include "Editor/FEditorToolkitHost.h"
#include "Viewer/FMeshViewerContent.h"
#include "Viewer/FBonePreview.h"
#include "Core/FilePath.h"
#include "Core/Math/FBox.h"
#include "Core/Math/FQuat.h"
#include "Core/Math/FRay.h"
#include "Core/Math/FVector4.h"
#include "Asset/FAssetManager.h"
#include "Asset/FAssetFile.h"
#include "Engine/FSceneInterface.h"
#include "Engine/FRenderTarget.h"
#include "Engine/FPrimitiveSceneProxy.h"
#include "Engine/FLightSceneProxy.h"
#include "Engine/FSceneViewFamily.h"
#include "Asset/UStaticMesh.h"
#include "Asset/USkeletalMesh.h"
#include "Asset/USkeleton.h"
#include "Engine/FPrimitiveDrawInterface.h"
#include "Engine/EditorMesh.h"
#include "Engine/MeshPicking.h"
#include "Asset/UMaterial.h"
#include "RenderCore/IRendererModule.h"

#include "EditorStyle.h"
#include "ClothEditing.h"
#include "PhysicsRuntime/UClothConfigNv.h"
#include "PhysicsRuntime/UClothingAssetCommon.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <d3d11.h>
#include <limits>

namespace Editor {

    /**
     * @brief 렌더러와 프로젝트 에셋 관리자를 연결합니다.
     */
    Editor::FMeshViewer::FMeshViewer(RenderCore::IRendererModule* Renderer, Asset::FAssetManager* Assets)
		: mAssets{Assets},
		  mViewer{Renderer, Assets, true} {
    }

    /**
     * @brief 미리보기 장면과 렌더 타깃을 해제합니다.
     */
    Editor::FMeshViewer::~FMeshViewer() = default;

    /**
     * @brief 에셋을 독립 미리보기 장면에 엽니다.
     */
    bool Editor::FMeshViewer::Open(const std::filesystem::path& Path, Core::FString& Error) {
        Error.clear();
        Asset::EAssetType Type{};

        if (mAssets == nullptr || !Asset::FAssetFile::GetType(mAssets->ResolvePath(Path, Error), Type, Error) || (Type != Asset::EAssetType::Mesh && Type != Asset::EAssetType::SkeletalMesh)) {
            return false;
        }

        const Core::FString MeshPath{Core::FilePath::ToGenericUtf8(Path)};
        const Core::FString SkeletonPath{Type == Asset::EAssetType::SkeletalMesh && MeshPath == mPath ? mSkeletonPath : Core::FString{}};
        CoreUObject::UObject* Object{Type == Asset::EAssetType::SkeletalMesh ? static_cast<CoreUObject::UObject*>(mAssets->LoadSkeletalMesh(Path, Error)) : mAssets->LoadMesh(Path, Error)};

        if (Object == nullptr) {
            return false;
        }

        if (!mViewer.SetAsset(Object, Type)) {
            Error = mViewer.GetError();

            return false;
        }

        mContent = static_cast<Viewer::FMeshViewerContent*>(mViewer.GetContent());

        if (Type == Asset::EAssetType::SkeletalMesh) {
            Asset::USkeleton* Skeleton{SkeletonPath.empty() ? mAssets->FindLegacySkeleton(Path) : mAssets->LoadSkeleton(Core::FilePath::FromUtf8(SkeletonPath), Error)};

            if ((!SkeletonPath.empty() && Skeleton == nullptr) || !mContent->SetSkeleton(Skeleton)) {
                Error = Error.empty() ? "Cannot prepare the skeleton preview" : Error;
                mViewer.Close();
                mContent = nullptr;
                mOpen = false;

                return false;
            }
        }

        mMesh = mContent->GetMesh();
        mSkeletalMesh = mContent->GetSkeletalMesh();
        mSkeleton = mContent->GetSkeleton();
        mSkeletonPath = SkeletonPath;
        mPoseBone.reset();
        mGizmo.EndDrag();
        mMaterialPaths.assign(mContent->GetMaterials().size(), Core::FString{});
        mPath = MeshPath;
        mError.clear();
        mOpen = true;
        mMouseCaptured = false;
        FrameMesh();

        return true;
    }

    /**
     * @brief 창의 열림 상태를 검사합니다.
     */
    void Editor::FMeshViewer::SetWindowId(Core::FString Id) {
        mWindowId = std::move(Id);
    }

    const Core::FString& Editor::FMeshViewer::GetWindowId() const {
        return mWindowId;
    }

    bool Editor::FMeshViewer::IsOpen() const {
        return mOpen;
    }

    void Editor::FMeshViewer::CloseRetired(const std::filesystem::path& Path) {
        if (!mOpen || mAssets == nullptr) {
            return;
        }

        Core::FString Error{};
        const auto Normalize{[this, &Error](const std::filesystem::path& Target) {
            return Core::FilePath::ToGenericUtf8(Core::FilePath::ToLower(Core::FilePath::FromUtf8(mAssets->GetRelativePath(Target, Error))));
        }};
        const Core::FString Root{Normalize(Path)};
        const auto Contains{[&](const Core::FString& Open) {
            if (Open.empty() || Root.empty()) {
                return false;
            }

            const Core::FString Relative{Normalize(Core::FilePath::FromUtf8(Open))};

            return Root == "." || Relative == Root || Relative.starts_with(Root + "/");
        }};

        if (Contains(mPath) || Contains(mSkeletonPath)) {
            mOpen = false;
        }
    }

    /**
     * @brief 전체 메시가 보이도록 카메라를 배치합니다.
     */
    void Editor::FMeshViewer::FrameMesh() {
        if (mContent != nullptr) {
            mViewer.GetCamera().SetRotation({-20.0f, 45.0f, 0.0f});
            mViewer.Fit();
            mNeedsFit = true;
        }
    }

    /**
     * @brief 현재 메시와 슬롯별 재질을 미리보기 장면에 반영합니다.
     */
    void Editor::FMeshViewer::UpdateScene() {
        mContent->Refresh();
    }

    /**
     * @brief 슬롯별 임시 재질 선택 UI를 표시합니다.
     */
    void Editor::FMeshViewer::DrawMaterials() {
        if (!ImGui::CollapsingHeader("Material Slots")) {
            return;
        }

        Core::FString Error{};
        const auto Paths{mAssets->GetAssetPaths(Error)};
        const auto& Slots{mMesh->GetMeshData().GetMaterialSlots()};

        for (Index_t Slot{}; Slot < Slots.size(); ++Slot) {
            ImGui::PushID(static_cast<int>(Slot));

            Core::FString Path{mMaterialPaths[Slot]};
            bool Changed{};

            if (ImGui::BeginCombo(Slots[Slot].GetName().c_str(), Path.empty() ? "Mesh Default" : Path.c_str())) {
                if (ImGui::Selectable("Mesh Default", Path.empty())) {
                    Path.clear();

                    Changed = true;
                }

                for (const auto& Candidate : Paths) {
                    Asset::EAssetType Type{};

                    if (Asset::FAssetFile::GetType(mAssets->ResolvePath(Candidate, Error), Type, Error) && Type == Asset::EAssetType::Material) {
                        const Core::FString Relative{Core::FilePath::ToGenericUtf8(Candidate)};

                        if (ImGui::Selectable(Relative.c_str(), Path == Relative)) {
                            Path = Relative;
                            Changed = true;
                        }
                    }
                }

                ImGui::EndCombo();
            }

            if (Changed) {
                const Core::FString Resolved{Path.empty() ? Slots[Slot].GetMaterialPath() : Path};
                Asset::UMaterial* Material{Resolved.empty() ? nullptr : mAssets->LoadMaterial(Core::FilePath::FromUtf8(Resolved), mError)};

                if (Resolved.empty() || Material != nullptr) {
                    mContent->GetMaterials()[Slot] = Material;
                    mMaterialPaths[Slot] = Path;

                    UpdateScene();
                }
            }

            ImGui::PopID();
        }
    }

    namespace {
        /**
         * @brief 메시가 기본으로 쓸 물리 에셋을 콤보 상자로 고르고 메시 파일에 저장합니다.
         * @param[in,out] Assets 에셋 관리자입니다.
         * @param[in,out] Mesh 물리 에셋 경로를 바꿀 스켈레탈 메시입니다.
         * @param[in] MeshPath 스켈레탈 메시의 콘텐츠 경로입니다.
         * @param[out] Error 실패 원인입니다.
         */
        void DrawPhysicsAssetPicker(Asset::FAssetManager& Assets, Asset::USkeletalMesh& Mesh, const Core::FString& MeshPath, Core::FString& Error) {
            const Core::FString Current{Mesh.GetPhysicsAssetPath()};

            if (!ImGui::BeginCombo("Physics Asset", Current.empty() ? "None (generated)" : Current.c_str())) {
                return;
            }

            Core::FString Selected{Current};
            bool Picked{};
            Core::FString ListError{};

            if (ImGui::Selectable("None (generated)", Current.empty())) {
                Selected.clear();
                Picked = true;
            }

            for (const auto& Candidate : Assets.GetAssetPaths(ListError)) {
                Asset::EAssetType Type{};

                if (Asset::FAssetFile::GetType(Assets.ResolvePath(Candidate, ListError), Type, ListError) && Type == Asset::EAssetType::PhysicsAsset) {
                    const Core::FString Relative{Core::FilePath::ToGenericUtf8(Candidate)};

                    if (ImGui::Selectable(Relative.c_str(), Relative == Current)) {
                        Selected = Relative;
                        Picked = true;
                    }
                }
            }

            ImGui::EndCombo();

            if (!Picked || Selected == Current) {
                return;
            }

            // 버전 1 메시는 본을 파일 안에 내장하는데 저장하면 새 형식으로 쓰여 본이 사라질 수 있으므로 막는다.
            if (Assets.FindLegacySkeleton(Core::FilePath::FromUtf8(MeshPath)) != nullptr) {
                Error = "This mesh uses an embedded skeleton, so its file cannot be saved safely. Set the physics asset on the component instead.";

                return;
            }

            Mesh.SetPhysicsAssetPath(Selected);
            Error.clear();

            if (!Assets.SaveSkeletalMesh(Core::FilePath::FromUtf8(MeshPath), Mesh, Error)) {
                Mesh.SetPhysicsAssetPath(Current);

                if (Error.empty()) {
                    Error = "Cannot save the skeletal mesh.";
                }
            }
        }
    }

    void Editor::FMeshViewer::DrawSkeletonPicker() {
        const char* Label{mSkeletonPath.empty() ? mSkeleton == nullptr ? "None" : "Embedded Skeleton" : mSkeletonPath.c_str()};

        if (!ImGui::BeginCombo("Skeleton", Label)) {
            return;
        }

        Core::FString SelectedPath{mSkeletonPath};
        Core::FString Error{};

        if (ImGui::Selectable("Default", SelectedPath.empty())) {
            SelectedPath.clear();
        }

        for (const auto& Candidate : mAssets->GetAssetPaths(Error)) {
            Asset::EAssetType Type{};

            if (Asset::FAssetFile::GetType(mAssets->ResolvePath(Candidate, Error), Type, Error) && Type == Asset::EAssetType::Skeleton) {
                const Core::FString Relative{Core::FilePath::ToGenericUtf8(Candidate)};

                if (ImGui::Selectable(Relative.c_str(), Relative == SelectedPath)) {
                    SelectedPath = Relative;
                }
            }
        }

        ImGui::EndCombo();

        if (SelectedPath == mSkeletonPath) {
            return;
        }

        Error.clear();

        Asset::USkeleton* Skeleton{SelectedPath.empty() ? mAssets->FindLegacySkeleton(Core::FilePath::FromUtf8(mPath)) : mAssets->LoadSkeleton(Core::FilePath::FromUtf8(SelectedPath), Error)};
        if ((!SelectedPath.empty() && Skeleton == nullptr) || !mContent->SetSkeleton(Skeleton)) {
            mError = Error.empty() ? "Cannot prepare the skeleton preview pose" : std::move(Error);

            return;
        }

        mSkeleton = mContent->GetSkeleton();
        mSkeletonPath = std::move(SelectedPath);
        mPoseBone.reset();
        mGizmo.EndDrag();
        mError.clear();

        UpdateScene();
    }

    /**
     * @brief 카메라·재질 조작 UI와 미리보기를 표시합니다.
     */
    void Editor::FMeshViewer::DrawUI(const Core::FInput& Input, bool Standalone) {
        if (!mOpen || mMesh == nullptr) {
            return;
        }

        ImGuiWindowFlags Flags{ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};

        if (Standalone) {
            const ImGuiViewport* Viewport{ImGui::GetMainViewport()};

            ImGui::SetNextWindowPos(Viewport->WorkPos);
            ImGui::SetNextWindowSize(Viewport->WorkSize);

            Flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        } else {
            ImGui::SetNextWindowSize({860.0f, 700.0f}, ImGuiCond_FirstUseEver);
        }

        // 탭에는 에셋 이름을 보여 주고 ### 뒤의 식별자는 유지합니다.
        const Core::FString Title{(Standalone ? "" : Editor::EditorStyle::TabIconPadding) + Core::FilePath::ToUtf8(Core::FilePath::FromUtf8(mPath).stem()) + mWindowId};

        const bool Expanded{Editor::EditorStyle::BeginEditorWindow(Title.c_str(), Standalone ? nullptr : &mOpen, Flags)};

        if (!Standalone) {
            Editor::EditorStyle::TabIcon(Editor::EditorStyle::EIcon::Local, IM_COL32(0, 220, 220, 255));
        }

        if (Expanded) {
            DrawProperties();

            if (mSkeleton != nullptr) {
                DrawPose();
                ImGui::SameLine();
            }

            DrawPreview(Input);
        } else {
            mMouseCaptured = false;
        }

        ImGui::End();
    }

    void Editor::FMeshViewer::InitializeToolkit(FEditorTab& Tab, const Core::FInput& Input) {
        mHostTab = &Tab;
        Tab.SetIcon(EEditorIcon::Local, IM_COL32(0, 220, 220, 255));
        FEditorToolkitHost& Host{Tab.GetToolkitHost()};
        Host.AddWidget("Preview", "Preview", [this, &Input](float) {
            if (mOpen && mMesh != nullptr) {
                DrawPreview(Input);
            }
        });
        Host.AddWidget("Details", "Details", [this](float) {
            DrawProperties();
        }, EEditorDockRegion::Details);

        if (mSkeletalMesh != nullptr) {
            Host.AddWidget("Skeleton", "Skeleton", [this](float) {
                if (mSkeleton != nullptr) {
                    DrawPose(true);
                }
            }, EEditorDockRegion::Left);
        }
    }

    void Editor::FMeshViewer::UpdateToolkit(float, bool) {
        mHostTab->SetName(EditorStyle::TabIconPadding + Core::FilePath::ToUtf8(Core::FilePath::FromUtf8(mPath).stem()) + mWindowId);

        if (!mHostTab->IsFocused()) {
            mMouseCaptured = false;
        }
    }

    void Editor::FMeshViewer::DrawProperties() {
        if (!mOpen || mMesh == nullptr) {
            return;
        }

        ImGui::TextUnformatted(mPath.c_str());
        ImGui::Text("Vertices: %u | Triangles: %u | SubMeshes: %zu | Materials: %zu", mMesh->GetVertexCount(), mMesh->GetIndexCount() / 3, mMesh->GetMeshData().GetSubMeshes().size(), mContent->GetMaterials().size());

        if (ImGui::Button("Frame Mesh")) {
            FrameMesh();
        }

        ImGui::SameLine();

        bool Wireframe{mViewMode == Engine::EViewMode::Wireframe};

        if (ImGui::Checkbox("Wireframe", &Wireframe)) {
            mViewMode = Wireframe ? Engine::EViewMode::Wireframe : Engine::EViewMode::Lit;
        }

        ImGui::SameLine();
        ImGui::TextDisabled(mSkeleton != nullptr ? "Click bone | Drag: orbit | Middle: pan | Wheel: zoom | F: frame | W/E: move/rotate" : "Drag: orbit | Middle: pan | Wheel: zoom | F: frame");
        DrawMaterials();

        if (mSkeletalMesh != nullptr) {
            DrawSkeletonPicker();
            DrawPhysicsAssetPicker(*mAssets, *mSkeletalMesh.Get(), mPath, mError);

            if (ImGui::CollapsingHeader("Clothing")) {
                DrawClothing();
            }
        }

        if (!mError.empty()) {
            ImGui::TextWrapped("%s", mError.c_str());
        }
    }

    /**
     * @brief 섹션마다 천 상태와 천 만들기, 천 에셋마다 물리 에셋과 설정, 메시 저장을 그립니다.
     */
    void Editor::FMeshViewer::DrawClothing() {
        Asset::USkeletalMesh& Mesh{*mSkeletalMesh.Get()};
        const auto& SubMeshes{Mesh.GetMeshData().GetSubMeshes()};

        // 섹션마다 천이 연결되어 있는지 보여 주고, 없으면 만들 수 있게 합니다.
        for (Index_t SubMeshIndex{}; SubMeshIndex < SubMeshes.size(); ++SubMeshIndex) {
            const Asset::FClothingSectionData* SectionData{Mesh.GetSectionClothingData(SubMeshIndex)};
            ImGui::PushID(static_cast<int>(SubMeshIndex));

            if (SectionData != nullptr && SectionData->IsValid()) {
                ImGui::Text("Section %zu: cloth asset %zu (%zu render verts)", SubMeshIndex, SectionData->mCorrespondClothAssetIndex, SectionData->mClothMappingData.size());
            } else {
                ImGui::Text("Section %zu: no cloth", SubMeshIndex);
                ImGui::SameLine();

                if (ImGui::SmallButton("Create Cloth")) {
                    if (mSkeleton == nullptr) {
                        mClothMessage = "Pick a skeleton first.";
                    } else {
                        // 메시가 쓰는 물리 에셋과 부딪히게 만들고, 저장한 물리 에셋이면 경로도 남깁니다.
                        Core::FString Error{};
                        PhysicsRuntime::UPhysicsAsset* PhysicsAsset{Mesh.GetPhysicsAssetPath().empty() ? Mesh.GetGeneratedPhysicsAsset(*mSkeleton.Get()) : mAssets->LoadPhysicsAsset(Core::FilePath::FromUtf8(Mesh.GetPhysicsAssetPath()), Error)};
                        mClothMessage = Editor::ClothEditing::CreateClothFromSection(Mesh, *mSkeleton.Get(), SubMeshIndex, PhysicsAsset, Mesh.GetPhysicsAssetPath(), Error) != nullptr ? "Created cloth on section " + std::to_string(SubMeshIndex) + ". Save the mesh to keep it." : Error;
                    }
                }
            }

            ImGui::PopID();
        }

        const auto& ClothingAssets{Mesh.GetMeshClothingAssets()};

        for (Index_t AssetIndex{}; AssetIndex < ClothingAssets.size(); ++AssetIndex) {
            PhysicsRuntime::UClothingAssetCommon& ClothingAsset{*ClothingAssets[AssetIndex]};
            PhysicsRuntime::UClothConfigNv* Config{ClothingAsset.GetClothConfig()};
            ImGui::PushID(static_cast<int>(1000 + AssetIndex));

            if (ImGui::TreeNode("ClothingAsset", "Clothing Asset %zu", AssetIndex)) {
                // 천 전용 물리 에셋을 고릅니다. 메시가 쓰는 물리 에셋과 따로 정할 수 있습니다.
                const Core::FString Current{ClothingAsset.mPhysicsAssetPath};

                if (ImGui::BeginCombo("Cloth Physics Asset", Current.empty() ? "None" : Current.c_str())) {
                    Core::FString ListError{};

                    if (ImGui::Selectable("None", Current.empty()) && mSkeleton != nullptr) {
                        ClothingAsset.SetPhysicsAsset(nullptr, Editor::ClothEditing::GetSkeletonBoneNames(*mSkeleton.Get()));
                        ClothingAsset.mPhysicsAssetPath.clear();
                    }

                    for (const auto& Candidate : mAssets->GetAssetPaths(ListError)) {
                        Asset::EAssetType Type{};

                        if (!Asset::FAssetFile::GetType(mAssets->ResolvePath(Candidate, ListError), Type, ListError) || Type != Asset::EAssetType::PhysicsAsset) {
                            continue;
                        }

                        const Core::FString Relative{Core::FilePath::ToGenericUtf8(Candidate)};

                        if (ImGui::Selectable(Relative.c_str(), Relative == Current) && mSkeleton != nullptr) {
                            Core::FString LoadError{};
                            PhysicsRuntime::UPhysicsAsset* PhysicsAsset{mAssets->LoadPhysicsAsset(Candidate, LoadError)};

                            if (PhysicsAsset != nullptr) {
                                ClothingAsset.SetPhysicsAsset(PhysicsAsset, Editor::ClothEditing::GetSkeletonBoneNames(*mSkeleton.Get()));
                                ClothingAsset.mPhysicsAssetPath = Relative;
                            } else {
                                mClothMessage = LoadError;
                            }
                        }
                    }

                    ImGui::EndCombo();
                }

                if (Config != nullptr && ImGui::TreeNode("Config")) {
                    bool Changed{};

                    for (const auto& [Name, Value] : Editor::ClothEditing::GetConfigFloats(*Config)) {
                        Changed = ImGui::DragFloat(Name.c_str(), Value, 0.01f) || Changed;
                    }

                    for (const auto& [Name, Value] : Editor::ClothEditing::GetConfigVectors(*Config)) {
                        Changed = ImGui::DragFloat3(Name.c_str(), &Value->x, 0.01f) || Changed;
                    }

                    Changed = ImGui::Checkbox("usegravityoverride", &Config->mUseGravityOverride) || Changed;

                    bool Accurate{Config->mClothingWindMethod == PhysicsRuntime::EClothingWindMethodNv::Accurate};

                    if (ImGui::Checkbox("accurate wind", &Accurate)) {
                        Config->mClothingWindMethod = Accurate ? PhysicsRuntime::EClothingWindMethodNv::Accurate : PhysicsRuntime::EClothingWindMethodNv::Legacy;
                        Changed = true;
                    }

                    // 천끼리 충돌 거리가 바뀌었을 수 있으므로 부딪힐 입자를 다시 고릅니다.
                    if (Changed) {
                        ClothingAsset.BuildSelfCollisionData();
                    }

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (!ClothingAssets.empty()) {
            ImGui::SeparatorText("Paint");
            ImGui::Checkbox("Paint Weights", &mClothPaintEnabled);
            ImGui::SliderInt("Paint Asset", &mClothPaintAsset, 0, static_cast<int>(ClothingAssets.size()) - 1);

            if (ImGui::BeginCombo("Paint Target", Editor::ClothEditing::GetPaintTargetName(mClothPaintTarget))) {
                for (int TargetIndex{}; TargetIndex < Editor::ClothEditing::NumPaintTargets; ++TargetIndex) {
                    if (ImGui::Selectable(Editor::ClothEditing::GetPaintTargetName(TargetIndex), TargetIndex == mClothPaintTarget)) {
                        mClothPaintTarget = TargetIndex;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::DragFloat("Paint Value", &mClothPaintValue, 0.005f, 0.0f, 100.0f, "%.3f");
            ImGui::DragFloat("Brush Radius (px)", &mClothPaintRadius, 1.0f, 2.0f, 300.0f, "%.0f");
            ImGui::TextDisabled("Left drag: paint | Shift + left drag: paint 0 | Right drag: orbit");
            ImGui::TextDisabled("Points show the bind pose. Red is 0, green is the largest value.");
        }

        ImGui::TextDisabled("Changes apply to cloth created after this. Re-place or replay the level to see them.");

        if (ImGui::Button("Save Mesh")) {
            Core::FString Error{};

            // 뼈를 파일 안에 넣은 예전 메시는 새 형식으로 저장하면 뼈가 사라지므로 막습니다.
            if (mAssets->FindLegacySkeleton(Core::FilePath::FromUtf8(mPath)) != nullptr) {
                mClothMessage = "This mesh uses an embedded skeleton, so its file cannot be saved safely.";
            } else if (mAssets->SaveSkeletalMesh(Core::FilePath::FromUtf8(mPath), Mesh, Error)) {
                mClothMessage = "Saved " + mPath + ".";
            } else {
                mClothMessage = "Cannot save the skeletal mesh. " + Error;
            }
        }

        if (!mClothMessage.empty()) {
            ImGui::TextWrapped("%s", mClothMessage.c_str());
        }
    }

    /**
     * @brief 천 칠하기를 쓸 수 있는 상태인지 검사합니다.
     */
    bool Editor::FMeshViewer::IsClothPainting() const {
        return mClothPaintEnabled && mSkeletalMesh != nullptr && mClothPaintAsset >= 0 && static_cast<Index_t>(mClothPaintAsset) < mSkeletalMesh->GetMeshClothingAssets().size();
    }

    /**
     * @brief 칠하기 대상 천 입자를 가중치 색으로 그리고, 왼쪽 버튼으로 붓 안의 입자에 값을 칠합니다.
     */
    void Editor::FMeshViewer::DrawClothPaint(const Engine::FSceneView& View, const FVector2& Origin, const FVector2& Mouse, bool Hovered, const Core::FInput& Input) {
        using Core::EKey;

        PhysicsRuntime::UClothingAssetCommon& ClothingAsset{*mSkeletalMesh->GetMeshClothingAssets()[static_cast<Index_t>(mClothPaintAsset)]};

        if (ClothingAsset.mLodData.empty()) {
            return;
        }

        PhysicsRuntime::FClothPhysicalMeshData& PhysMesh{ClothingAsset.mLodData[0].mPhysicalMeshData};
        PhysicsRuntime::FPointWeightMap& Map{PhysMesh.AddWeightMap(Editor::ClothEditing::GetPaintTarget(mClothPaintTarget))};

        // 처음 칠하는 맵은 입자 수만큼 0으로 채웁니다.
        if (Map.mValues.size() != PhysMesh.mVertices.size()) {
            Map.mValues.resize(PhysMesh.mVertices.size(), 0.0f);
        }

        float MaxValue{1.e-4f};

        for (const float Value : Map.mValues) {
            MaxValue = std::max(MaxValue, Value);
        }

        const bool Stroke{Hovered && Input.IsDown(EKey::MouseLeft) && !Input.IsDown(EKey::MouseRight) && !Input.IsDown(EKey::MouseMiddle)};
        const float PaintValue{Input.IsDown(EKey::Shift) ? 0.0f : mClothPaintValue};
        const float RadiusSquared{mClothPaintRadius * mClothPaintRadius};
        ImDrawList* const DrawList{ImGui::GetWindowDrawList()};

        for (Index_t VertIndex{}; VertIndex < PhysMesh.mVertices.size(); ++VertIndex) {
            // 입자를 화면 좌표로 투영합니다. 카메라 뒤의 입자는 건너뜁니다.
            const FVector4 Clip{FVector4{PhysMesh.mVertices[VertIndex], 1.0f} * View.mViewProjection};

            if (Clip.w <= 1.e-6f) {
                continue;
            }

            const FVector2 Screen{(Clip.x / Clip.w * 0.5f + 0.5f) * View.mSize.x, (0.5f - Clip.y / Clip.w * 0.5f) * View.mSize.y};
            const float DistanceSquared{(Screen.x - Mouse.x) * (Screen.x - Mouse.x) + (Screen.y - Mouse.y) * (Screen.y - Mouse.y)};

            if (Stroke && DistanceSquared <= RadiusSquared) {
                Map.mValues[VertIndex] = PaintValue;
                mClothPaintDirty = true;
            }

            // 0은 빨강, 가장 큰 값은 초록으로 그립니다.
            const float Ratio{std::clamp(Map.mValues[VertIndex] / MaxValue, 0.0f, 1.0f)};
            const ImU32 Color{IM_COL32(static_cast<int>(255.0f * (1.0f - Ratio)), static_cast<int>(255.0f * Ratio), 60, 255)};

            DrawList->AddCircleFilled(ImVec2{Origin.x + Screen.x, Origin.y + Screen.y}, 2.5f, Color);
        }

        if (Hovered) {
            DrawList->AddCircle(ImVec2{Origin.x + Mouse.x, Origin.y + Mouse.y}, mClothPaintRadius, IM_COL32(255, 255, 255, 200), 0, 1.5f);
        }

        // 고정 입자와 역질량 계산은 무거우므로 버튼을 놓을 때 한 번 반영합니다.
        if (mClothPaintDirty && !Input.IsDown(EKey::MouseLeft)) {
            Editor::ClothEditing::ApplyPaintedWeights(*mSkeletalMesh.Get(), ClothingAsset);
            mClothPaintDirty = false;
            mClothMessage = "Painted weights applied. Save the mesh to keep them.";
        }
    }

    /**
     * @brief 카메라 입력을 처리하고 공통 렌더 경로로 메시와 본 도형을 그리며 아이콘·스프라이트·텍스트 표시는 제외합니다.
     */
    void Editor::FMeshViewer::DrawPreview(const Core::FInput& Input) {
        const ImVec2 Size{ImGui::GetContentRegionAvail()};

        if (Size.x < 1.0f || Size.y < 1.0f) {
            mMouseCaptured = false;

            return;
        }

        const Uint32 Width{static_cast<Uint32>(Size.x)};
        const Uint32 Height{static_cast<Uint32>(Size.y)};

        if (!mViewer.Resize(Width, Height)) {
            return;
        }

        if (mNeedsFit) {
            mNeedsFit = !mViewer.Fit();
        }

        // 이미지는 입력을 소유하지 않아 드래그가 창 이동으로 처리되므로 버튼이 마우스를 받게 합니다.
        ImGui::InvisibleButton("##Preview", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

        const bool Hovered{ImGui::IsItemHovered()};

        using Core::EKey;

        const FVector2 Mouse{Input.GetMousePosition()};
        const FVector2 Delta{Input.GetMouseDelta()};
        const FVector2 Extent{Size.x, Size.y};
        const ImVec2 Origin{ImGui::GetItemRectMin()};

        ImGui::GetWindowDrawList()->AddImage(ImTextureRef{static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mViewer.GetNativeTexture()))}, Origin, ImGui::GetItemRectMax());

        const Engine::FSceneView InputView{CalcSceneView(Extent)};
        const FVector2 PreviewMouse{Mouse.x - Origin.x, Mouse.y - Origin.y};
        const bool GizmoActive{mPoseBone && UpdateBoneGizmo(*mPoseBone, Input, InputView, PreviewMouse, Hovered)};
        const Core::TArray<Viewer::FBoneShape> BoneShapes{mSkeleton != nullptr && mShowBones ? Viewer::BuildBoneShapes(mSkeleton->GetBones(), mContent->GetPose().GetGlobalTransforms(), mViewer.GetCamera().GetRadius()) : Core::TArray<Viewer::FBoneShape>{}};
        // 천을 칠하는 동안 왼쪽 버튼은 붓으로 쓰고, 뼈 선택과 카메라 회전에는 쓰지 않습니다.
        const bool Painting{IsClothPainting()};
        const bool CanPickBone{Hovered && !mMouseCaptured && !GizmoActive && !BoneShapes.empty() && !Painting};
        const bool LeftClick{Input.WasPressed(EKey::MouseLeft) && !Input.IsDown(EKey::MouseRight) && !Input.IsDown(EKey::MouseMiddle)};
        const bool PickedBone{CanPickBone && LeftClick && PickBone(InputView, PreviewMouse, BoneShapes)};

        if (PickedBone) {
            ImGui::SetWindowFocus();
        } else if (!GizmoActive && Hovered && ((Input.WasPressed(EKey::MouseLeft) && !Painting) || Input.WasPressed(EKey::MouseRight) || Input.WasPressed(EKey::MouseMiddle))) {
            mMouseCaptured = true;

            ImGui::SetWindowFocus();
        }

        if (!ImGui::IsWindowFocused() || (!Input.IsDown(EKey::MouseLeft) && !Input.IsDown(EKey::MouseRight) && !Input.IsDown(EKey::MouseMiddle))) {
            mMouseCaptured = false;
        }

        if (mMouseCaptured) {
            if (Input.IsDown(EKey::MouseMiddle)) {
                mViewer.GetCamera().Pan(Delta, Size.y);
            } else {
                mViewer.GetCamera().Orbit(Delta);
            }
        }

        if (Hovered && Input.GetMouseWheel() != 0.0f) {
            mViewer.GetCamera().Zoom(Input.GetMouseWheel());
        }

        if (Hovered && !ImGui::GetIO().WantTextInput && Input.WasPressed(EKey::F)) {
            FrameMesh();
        }

        if (mSkeleton != nullptr && Hovered && !ImGui::GetIO().WantTextInput && !mGizmo.IsDragging()) {
            if (Input.WasPressed(EKey::W)) {
                mGizmoMode = Editor::EGizmoMode::Translate;
            } else if (Input.WasPressed(EKey::E)) {
                mGizmoMode = Editor::EGizmoMode::Rotate;
            }
        }

        if (Painting) {
            DrawClothPaint(InputView, FVector2{Origin.x, Origin.y}, PreviewMouse, Hovered, Input);
        }

        const Engine::FSceneView View{CalcSceneView(Extent)};

        Engine::FPrimitiveDrawInterface Bones{};

        for (const Viewer::FBoneShape& Shape : BoneShapes) {
            const float Gray{Shape.GetMesh() == Engine::EEditorMesh::Sphere ? 0.76f : 0.66f};
            const FVector4 Color{Shape.GetBone() == mPoseBone ? FVector4{0.26f, 0.66f, 1.0f, 1.0f} : FVector4{Gray, Gray, Gray, 1.0f}};

            Bones.DrawMesh({Shape.GetMesh(), Shape.GetWorld(), Color});
        }

        Engine::FPrimitiveDrawInterface Gizmo{};

        if (mPoseBone) {
            mGizmo.BuildHandles(View, GetBonePlacement(*mPoseBone), Gizmo);
            mGizmo.DrawFeedback(Gizmo);
        }

        mViewer.SetViewMode(mViewMode);
        mViewer.Render(mPoseBone ? &Gizmo : nullptr, BoneShapes.empty() ? nullptr : &Bones);
    }

    /**
     * @brief 미리보기 옆에 본 계층 선택·기즈모 설정·포즈 복원 패널을 표시합니다.
     */
    void Editor::FMeshViewer::DrawPose(bool FillWidth) {
        const float Scale{ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi};

        if (ImGui::BeginChild("Skeleton", {FillWidth ? 0.0f : 320.0f * Scale, 0.0f}, ImGuiChildFlags_Borders)) {
            const auto& Bones{mSkeleton->GetBones()};

            ImGui::Text("Bones: %zu", Bones.size());
            ImGui::Checkbox("Show Bones", &mShowBones);

            // 아래쪽 기즈모 설정 영역을 남기고 나머지 높이를 계층 트리에 할당합니다.
            const float ControlsHeight{ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() * 2.0f};

            if (ImGui::BeginChild("Bone Hierarchy", {0.0f, -ControlsHeight}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
                Core::TArray<Core::TArray<Index_t>> Children(Bones.size());

                for (Index_t Index{}; Index < Bones.size(); ++Index) {
                    const Int32 Parent{static_cast<Int32>(Bones[Index].GetParentIndex())};

                    if (Parent >= 0) {
                        Children[Parent].push_back(Index);
                    }
                }

                // 본 계층은 깊으므로 들여쓰기를 줄여 깊은 본의 이름까지 패널 안에 표시합니다.
                ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetStyle().IndentSpacing / 3.0f);

                for (Index_t Index{}; Index < Bones.size(); ++Index) {
                    if (Bones[Index].GetParentIndex() < 0) {
                        DrawBoneNode(Index, Children);
                    }
                }

                ImGui::PopStyleVar();
            }

            ImGui::EndChild();
            ImGui::TextUnformatted(mPoseBone ? Bones[*mPoseBone].GetName().c_str() : "Select a bone");

            if (ImGui::RadioButton("Rotate", mGizmoMode == Editor::EGizmoMode::Rotate)) {
                mGizmoMode = Editor::EGizmoMode::Rotate;
            }

            ImGui::SameLine();

            if (ImGui::RadioButton("Move", mGizmoMode == Editor::EGizmoMode::Translate)) {
                mGizmoMode = Editor::EGizmoMode::Translate;
            }

            ImGui::SameLine();

            bool Local{mGizmoSpace == Editor::EGizmoSpace::Local};

            if (ImGui::Checkbox("Local", &Local)) {
                mGizmoSpace = Local ? Editor::EGizmoSpace::Local : Editor::EGizmoSpace::World;
            }

            ImGui::BeginDisabled(!mPoseBone);

            if (ImGui::Button("Reset Bone") && mPoseBone && mContent->GetPose().SetLocalTransform(*mSkeletalMesh, *mSkeleton, *mPoseBone, Bones[*mPoseBone].GetBindPose())) {
                UpdateScene();
            }

            ImGui::EndDisabled();
            ImGui::SameLine();

            if (ImGui::Button("Reset Bind Pose") && mContent->GetPose().Reset(*mSkeletalMesh, *mSkeleton)) {
                UpdateScene();
            }
        }

        ImGui::EndChild();
    }

    /**
     * @brief 본 하나와 그 자손을 계층 트리 노드로 표시하고 클릭하면 조작 대상으로 선택합니다.
     */
    void Editor::FMeshViewer::DrawBoneNode(Index_t Bone, const Core::TArray<Core::TArray<Index_t>>& Children) {
        const auto& BoneChildren{Children[Bone]};
        const ImGuiTreeNodeFlags Flags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen | (Bone == mPoseBone ? ImGuiTreeNodeFlags_Selected : 0) | (BoneChildren.empty() ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : 0)};

        // 본 이름은 중복될 수 있으므로 번호를 트리 노드 식별자로 사용합니다.
        const bool Open{ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(Bone)), Flags, "%s", mSkeleton->GetBones()[Bone].GetName().c_str())};

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            mGizmo.EndDrag();
            mPoseBone = Bone;
        }

        if (Open && !BoneChildren.empty()) {
            for (const Index_t Child : BoneChildren) {
                DrawBoneNode(Child, Children);
            }

            ImGui::TreePop();
        }
    }

    bool Editor::FMeshViewer::PickBone(const Engine::FSceneView& View, const FVector2& Mouse, const Core::TArray<Viewer::FBoneShape>& Shapes) {
        FRay Ray{};

        if (!View.Deproject(Mouse, Ray)) {
            return false;
        }

        const Viewer::FBoneShape* Hit{};
        float Closest{std::numeric_limits<float>::max()};

        for (auto Shape{Shapes.rbegin()}; Shape != Shapes.rend(); ++Shape) {
            if (Engine::RaycastMesh(&Engine::GetEditorMesh(Shape->GetMesh()), Shape->GetWorld(), Ray, Closest)) {
                Hit = &*Shape;
            }
        }

        if (Hit == nullptr) {
            return false;
        }

        const FVector4 Clip{FVector4{Ray.mOrigin + Ray.mDirection * Closest, 1.0f} * View.mViewProjection};

        if (Clip.z > Clip.w) {
            return false;
        }

        mPoseBone = Hit->GetBone();
        mGizmo.EndDrag();

        return true;
    }

    /**
     * @brief 현재 궤도 카메라의 렌더링과 입력 해석에 쓸 뷰를 계산합니다.
     */
    Engine::FSceneView Editor::FMeshViewer::CalcSceneView(const FVector2& Size) const {
        return mViewer.GetCamera().CalcSceneView(Size);
    }

    /**
     * @brief 선택한 본의 현재 전역 포즈로 기즈모 배치를 구성합니다.
     */
    Editor::FGizmoPlacement Editor::FMeshViewer::GetBonePlacement(Index_t Bone) const {
        const FMatrix& Global{mContent->GetPose().GetGlobalTransforms()[Bone]};
        Editor::FGizmoPlacement Placement{};
        Placement.mPivot = Global.TransformPosition({});
        Placement.mMode = mGizmoMode;

        // 본 행렬에는 스케일이 있을 수 있으므로 축을 정규화하고, 퇴화한 축은 월드 축으로 대체합니다.
        for (Index_t Index{}; Index < 3; ++Index) {
            const FVector Axis{Global.GetAxis(Index).Normalized()};

            Placement.mAxes[Index] = mGizmoSpace == Editor::EGizmoSpace::Local && !Axis.IsNearlyZero() ? Axis : Editor::FGizmoCore::GetUnitAxes()[Index];
        }

        return Placement;
    }

    /**
     * @brief 기즈모의 호버와 드래그 입력을 처리합니다.
     */
    bool Editor::FMeshViewer::UpdateBoneGizmo(Index_t Bone, const Core::FInput& Input, const Engine::FSceneView& View, const FVector2& Mouse, bool Hovered) {
        using Core::EKey;

        if (mGizmo.IsDragging()) {
            if (Input.WasPressed(EKey::Escape)) {
                mGizmo.EndDrag();

                if (mContent->GetPose().SetLocalTransform(*mSkeletalMesh, *mSkeleton, Bone, mDragStartLocal)) {
                    UpdateScene();
                }

                return true;
            }

            if (mGizmo.UpdateDrag(Mouse)) {
                ApplyBoneGizmo(Bone);
            }

            if (!Input.IsDown(EKey::MouseLeft)) {
                mGizmo.EndDrag();
            }

            return true;
        }

        const bool CanHover{Hovered && !mMouseCaptured && !Input.IsDown(EKey::MouseRight) && !Input.IsDown(EKey::MouseMiddle)};
        const Editor::FGizmoPlacement Placement{GetBonePlacement(Bone)};
        const Engine::FHitProxy Hit{CanHover ? mGizmo.HitTest(Mouse, View, Placement) : Engine::FHitProxy{}};
        const Int32 Axis{static_cast<Int32>(Hit.mType == Engine::EHitProxyType::GizmoAxis ? Hit.mAxis : -1)};

        mGizmo.SetHoveredAxis(Axis);

        if (Axis < 0 || !Input.WasPressed(EKey::MouseLeft) || !mGizmo.BeginDrag(Axis, Mouse, View, Placement)) {
            return false;
        }

        mDragStartGlobal = mContent->GetPose().GetGlobalTransforms()[Bone];
        mDragStartLocal = mContent->GetPose().GetLocalTransforms()[Bone];

        ImGui::SetWindowFocus();

        return true;
    }

    /**
     * @brief 기즈모의 드래그 결과를 선택한 본의 부모 기준 로컬 변환으로 바꾸어 적용합니다.
     */
    void Editor::FMeshViewer::ApplyBoneGizmo(Index_t Bone) {
        FMatrix Delta{FMatrix::Translation(mGizmo.GetTranslation())};

        if (mGizmo.GetDragMode() == Editor::EGizmoMode::Rotate) {
            const FVector& Pivot{mGizmo.GetStartPivot()};

            Delta = FMatrix::Translation(-Pivot) * FMatrix::Rotation(mGizmo.GetRotation()) * FMatrix::Translation(Pivot);
        }

        const Int32 Parent{static_cast<Int32>(mSkeleton->GetBones()[Bone].GetParentIndex())};
        FMatrix ParentInverse{FMatrix::Identity()};

        if (Parent >= 0 && !mContent->GetPose().GetGlobalTransforms()[Parent].TryInverse(ParentInverse)) {
            return;
        }

        // 행 벡터 규약에서 전역 변환은 Local * ParentGlobal이므로 부모의 역행렬을 오른쪽에 곱해 로컬 변환을 되찾습니다.
        FMatrix Local{mDragStartGlobal * Delta * ParentInverse};

        // 역행렬의 부동소수점 오차가 아핀 검사에 걸리지 않도록 마지막 열을 고정합니다.
        for (Index_t Row{}; Row < 3; ++Row) {
            Local(Row, 3) = 0.0f;
        }

        Local(3, 3) = 1.0f;

        if (mContent->GetPose().SetLocalTransform(*mSkeletalMesh, *mSkeleton, Bone, Local)) {
            UpdateScene();
        }
    }

}
