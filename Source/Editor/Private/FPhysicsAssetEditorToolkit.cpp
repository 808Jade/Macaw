/**
 * @file FPhysicsAssetEditorToolkit.cpp
 * @brief 스켈레탈 메시의 물리 에셋을 미리보기와 함께 편집하는 에셋 툴킷을 구현합니다.
 * @ingroup Editor
 */

#include "FPhysicsAssetEditorToolkit.h"

#include "EditorStyle.h"
#include "FPhysicsAssetDrawing.h"
#include "FPhysicsAssetViewerContent.h"
#include "Asset/FAssetFile.h"
#include "Asset/FAssetManager.h"
#include "Asset/USkeletalMesh.h"
#include "Asset/USkeleton.h"
#include "Core/FilePath.h"
#include "Core/Math/FQuat.h"
#include "Core/Math/FRay.h"
#include "Editor/EEditorIcon.h"
#include "Editor/FEditorContext.h"
#include "Editor/FEditorToolkitHost.h"
#include "Engine/Component/USkeletalMeshComponent.h"
#include "Engine/FPrimitiveDrawInterface.h"
#include "Engine/FSceneView.h"
#include "Engine/FProject.h"
#include "Engine/UWorld.h"
#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/UBodySetup.h"
#include "PhysicsRuntime/UPhysicsAsset.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

using namespace Editor;

namespace {
    /**
     * @brief 주 축과 보조 축으로 만든 프레임 방향을 오일러 회전(도)으로 바꿉니다.
     * @param[in] Primary 프레임의 X축으로 쓰는 주 축입니다.
     * @param[in] Secondary 프레임의 Y축을 정하는 보조 축입니다.
     * @return 프레임 방향의 회전이며 두 축이 평행하면 회전 없음을 반환합니다.
     */
    FRotator AxesToRotator(const FVector& Primary, const FVector& Secondary) {
        const FVector Cross{Primary.Cross(Secondary)};

        if (Primary.LengthSquared() < 1e-8f || Cross.LengthSquared() < 1e-8f) {
            return FRotator{};
        }

        const FVector AxisX{Primary.Normalized()};
        const FVector AxisZ{Cross.Normalized()};
        const FVector AxisY{AxisZ.Cross(AxisX)};
        const FVector Axes[]{AxisX, AxisY, AxisZ};
        FMatrix Matrix{FMatrix::Identity()};

        // 관절 프레임을 만드는 방식과 같이 축을 행으로 쌓는다.
        for (Index_t Row{}; Row < 3; ++Row) {
            Matrix(Row, 0) = Axes[Row].x;
            Matrix(Row, 1) = Axes[Row].y;
            Matrix(Row, 2) = Axes[Row].z;
        }

        FTransform Result{};

        return Matrix.TryGetTransform(Result) ? FRotator{Result.mRotation} : FRotator{};
    }
}

FPhysicsAssetEditorToolkit::FPhysicsAssetEditorToolkit(FEditorContext* Context, const std::filesystem::path& AssetPath, const std::filesystem::path& MeshPath, Asset::USkeletalMesh* Mesh, Asset::USkeleton* Skeleton, const Core::FString& PhysicsAssetPath)
    : FAssetEditorToolkit{AssetPath, Asset::EAssetType::PhysicsAsset, "###PhysicsAssetEditor:" + Core::FilePath::ToGenericUtf8(AssetPath)},
      mContext{Context},
      mMeshPath{MeshPath},
      mPhysicsAssetPath{PhysicsAssetPath},
      mMesh{Mesh},
      mViewer{Context->GetRenderer(), Context->GetProject()->GetAssets(), true} {
    GetTab().SetIcon(EEditorIcon::Skeleton, IM_COL32(255, 120, 90, 255));

    FEditorToolkitHost& Host{GetToolkitHost()};
    Host.AddWidget("Preview", "Viewport", [this](float) {
        DrawViewport();
    });
    Host.GetTabManager().FindTab("Preview")->SetWindowFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    Host.AddWidget("Hierarchy", "Hierarchy", [this](float) {
        DrawHierarchy();
    },
        EEditorDockRegion::Left);
    Host.AddWidget("Details", "Details", [this](float) {
        DrawDetails();
    },
        EEditorDockRegion::Details);

    Core::FString DefaultPath{};

    if (Skeleton == nullptr) {
        Skeleton = FindDefaultSkeleton(DefaultPath);
    }

    if (Skeleton != nullptr) {
        if (BuildPreview(Skeleton)) {
            mSkeletonPath = std::move(DefaultPath);
        } else {
            mSkeleton = nullptr;
        }
    }
}

FPhysicsAssetEditorToolkit::~FPhysicsAssetEditorToolkit() = default;

void FPhysicsAssetEditorToolkit::UpdateEditor(float) {
    // 저장하지 않은 수정이 있으면 제목 끝에 *를 붙인다.
    GetTab().SetName(EditorStyle::TabIconPadding + Core::FilePath::ToUtf8(GetAssetPath().stem()) + " Physics" + (mDirty ? " *" : "") + "###PhysicsAssetEditor:" + Core::FilePath::ToGenericUtf8(GetAssetPath()));
}

bool FPhysicsAssetEditorToolkit::BuildPreview(Asset::USkeleton* Skeleton) {
    // 바닥 바디는 이전 물리 장면에 속하므로 콘텐츠를 바꾸기 전에 해제한다.
    mFloorBody.reset();
    mSimulating = false;

    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};
    auto Content{std::make_unique<FPhysicsAssetViewerContent>(mContext->GetRenderer(), Assets)};

    // 미리보기 월드는 엔진이 어댑트한 월드가 아니므로 메인 월드가 쓰는 PhysX 런타임을 빌려 준다.
    const Engine::UWorld* MainWorld{mContext->GetWorld()};
    PhysicsRuntime::FPhysicsRuntime* Runtime{MainWorld != nullptr ? MainWorld->GetPhysicsRuntime() : nullptr};

    if (!Content->SetMesh(mMesh, Skeleton, Runtime, mError)) {
        return false;
    }

    FPhysicsAssetViewerContent* Raw{Content.get()};

    if (!mViewer.SetContent(std::move(Content))) {
        mError = mViewer.GetError();

        return false;
    }

    mContent = Raw;
    mSkeleton = Skeleton;
    mNeedsFrame = true;
    mError.clear();
    RebuildBoneTree();

    // 물리 에셋 파일을 편집할 때는 미리보기 컴포넌트가 그 파일을 쓰도록 지정한다.
    if (!mPhysicsAssetPath.empty() && !Raw->GetComponent()->SetPhysicsAssetOverride(mPhysicsAssetPath)) {
        mError = "Cannot load the physics asset: " + mPhysicsAssetPath;

        return false;
    }

    return true;
}

void FPhysicsAssetEditorToolkit::AssignToMesh() {
    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};

    if (mPhysicsAssetPath.empty() || mMesh == nullptr) {
        return;
    }

    if (Assets->FindLegacySkeleton(mMeshPath) != nullptr) {
        mError = "This mesh uses an embedded skeleton, so its file cannot be saved safely. Set the physics asset on the component instead.";

        return;
    }

    const Core::FString Previous{mMesh->GetPhysicsAssetPath()};
    Core::FString Error{};

    mMesh->SetPhysicsAssetPath(mPhysicsAssetPath);

    if (!Assets->SaveSkeletalMesh(mMeshPath, *mMesh, Error)) {
        mMesh->SetPhysicsAssetPath(Previous);
        mError = Error.empty() ? "Cannot save the skeletal mesh." : Error;

        return;
    }

    mError.clear();
    mContext->AddLog("Assigned physics asset to the mesh: " + mPhysicsAssetPath);
}

void FPhysicsAssetEditorToolkit::SaveAsset() {
    Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};
    PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};

    if (Component == nullptr || PhysicsAsset == nullptr) {
        mError = "There is no physics asset to save.";

        return;
    }

    const bool NewFile{mPhysicsAssetPath.empty()};
    const Core::FString TargetPath{NewFile ? Core::FilePath::ToGenericUtf8(PhysicsAssetEditing::GetDefaultPhysicsAssetPath(mMeshPath)) : mPhysicsAssetPath};
    Core::FString Error{};

    if (!mContext->GetProject()->GetAssets()->SavePhysicsAsset(Core::FilePath::FromUtf8(TargetPath), *PhysicsAsset, Error)) {
        mError = Error.empty() ? "Cannot save the physics asset." : Error;

        return;
    }

    mError.clear();
    mDirty = false;

    // 처음 저장하면 이후 저장이 같은 에셋 객체를 대상으로 하도록 저장한 파일을 미리보기에 연결한다.
    if (NewFile) {
        mPhysicsAssetPath = TargetPath;

        mFloorBody.reset();
        mSimulating = false;

        if (!Component->SetPhysicsAssetOverride(mPhysicsAssetPath)) {
            mError = "Saved, but cannot reload the physics asset: " + mPhysicsAssetPath;
        }
    }

    mContext->AddLog("Saved physics asset: " + TargetPath);
}

void FPhysicsAssetEditorToolkit::RebuildBoneTree() {
    mBoneChildren.clear();
    mRootBones.clear();
    mSelectedBone = InvalidIndex;
    mSelectedBody = InvalidIndex;
    mSelectedShape = PhysicsAssetEditing::FShapeRef{};
    mSelectedConstraint = InvalidIndex;

    if (mSkeleton == nullptr) {
        return;
    }

    const auto& Bones{mSkeleton->GetBones()};

    mBoneChildren.resize(Bones.size());

    for (Index_t Bone{}; Bone < Bones.size(); ++Bone) {
        const Int32 Parent{Bones[Bone].GetParentIndex()};

        if (Parent >= 0 && static_cast<Index_t>(Parent) < Bones.size()) {
            mBoneChildren[static_cast<Index_t>(Parent)].push_back(Bone);
        } else {
            mRootBones.push_back(Bone);
        }
    }
}

PhysicsRuntime::UPhysicsAsset* FPhysicsAssetEditorToolkit::GetPhysicsAsset() const {
    const Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};

    return Component != nullptr ? Component->GetPhysicsAsset() : nullptr;
}

void FPhysicsAssetEditorToolkit::StartSimulation() {
    Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};
    Engine::UWorld* World{mContent != nullptr ? mContent->GetWorld() : nullptr};
    PhysicsRuntime::FPhysicsScene* Scene{World != nullptr ? World->GetPhysicsScene() : nullptr};

    if (Component == nullptr || Scene == nullptr) {
        return;
    }

    // 애니메이션 자세의 키네마틱 바디로 다시 만든 뒤 시뮬레이션으로 바꾼다.
    Component->ResetPose();
    Component->RecreatePhysicsState();
    // 바닥은 미리보기가 그리는 바닥과 같은 기준(메시의 바인드 범위)으로 만든다. 현재 포즈의 범위를 쓰면
    // 재시작할 때 이전 시뮬레이션에서 떨어진 포즈 아래에 바닥이 생긴다.
    mFloorBounds = mMesh->GetBounds();
    mFloorBody = PhysicsAssetEditing::CreateFloorBody(*Scene, mFloorBounds);
    Component->SetAllBodiesSimulatePhysics(true);
    mSimulating = true;
}

void FPhysicsAssetEditorToolkit::StopSimulation() {
    Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};

    mFloorBody.reset();
    mSimulating = false;

    // 시뮬레이션이 바꾼 뼈 포즈를 기준 자세로 되돌린 뒤 키네마틱 바디를 다시 만든다.
    if (Component != nullptr) {
        Component->ResetPose();
        Component->RecreatePhysicsState();
    }
}

void FPhysicsAssetEditorToolkit::BuildBodyLines(Engine::FPrimitiveDrawInterface& Draw) const {
    const Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};
    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{Component != nullptr ? Component->GetPhysicsAsset() : nullptr};

    if (PhysicsAsset == nullptr) {
        return;
    }

    // 시뮬레이션 중에는 뼈 자세 대신 PhysX 바디의 현재 자세로 그린다.
    Core::TArray<PhysicsAssetEditing::FBodyPose> Poses{};

    if (mSimulating) {
        Engine::UWorld* World{mContent->GetWorld()};

        if (World != nullptr && World->GetPhysicsScene() != nullptr) {
            PhysicsAssetEditing::CollectBoneBodyPoses(*World->GetPhysicsScene(), Poses);
        }

        // 바닥은 윗면 사각형과 대각선으로 표시한다.
        FVector HalfExtent{};
        const FTransform Floor{PhysicsAssetEditing::GetFloorTransform(mFloorBounds, HalfExtent)};
        const FVector Top{Floor.mLocation + FVector{0.0f, 0.0f, HalfExtent.z}};
        const FVector Corners[4]{Top + FVector{-HalfExtent.x, -HalfExtent.y, 0.0f}, Top + FVector{HalfExtent.x, -HalfExtent.y, 0.0f}, Top + FVector{HalfExtent.x, HalfExtent.y, 0.0f}, Top + FVector{-HalfExtent.x, HalfExtent.y, 0.0f}};

        for (int Corner{}; Corner < 4; ++Corner) {
            Draw.DrawLine(Corners[Corner], Corners[(Corner + 1) % 4], FVector{0.5f, 0.5f, 0.5f}, false, 1.5f);
        }

        Draw.DrawLine(Corners[0], Corners[2], FVector{0.35f, 0.35f, 0.35f}, false, 1.0f);
        Draw.DrawLine(Corners[1], Corners[3], FVector{0.35f, 0.35f, 0.35f}, false, 1.0f);
    }

    for (Index_t Index{}; Index < PhysicsAsset->mSkeletalBodySetups.size(); ++Index) {
        const PhysicsRuntime::UBodySetup* Setup{PhysicsAsset->mSkeletalBodySetups[Index].get()};
        FTransform BoneWorld{};

        if (Setup == nullptr) {
            continue;
        }

        if (mSimulating) {
            const auto Found{std::find_if(Poses.begin(), Poses.end(), [Index](const PhysicsAssetEditing::FBodyPose& Pose) {
                return Pose.mBodyIndex == Index;
            })};

            if (Found == Poses.end()) {
                continue;
            }

            BoneWorld = Found->mWorld;
        } else {
            // 바디를 만들 때와 같은 뼈 월드 변환(스케일 포함)을 써서 실제 충돌 모양과 같은 크기로 그린다.
            const Index_t BoneIndex{Component->GetBoneIndex(Setup->GetBoneName())};

            if (BoneIndex == InvalidIndex || !Component->GetBoneWorldTransform(BoneIndex, BoneWorld)) {
                continue;
            }
        }

        const bool Selected{Index == mSelectedBody};

        PhysicsAssetDrawing::DrawBodySetup(Draw, *Setup, BoneWorld, Selected ? FVector{1.0f, 0.65f, 0.15f} : FVector{0.35f, 0.6f, 1.0f}, Selected ? 2.5f : 1.2f);
    }
}

void FPhysicsAssetEditorToolkit::DrawHierarchy() {
    if (mSkeleton == nullptr || mContent == nullptr || !mContent->IsReady()) {
        ImGui::TextDisabled("No preview");

        return;
    }

    ImGui::Checkbox("Show Bodies", &mShowBodies);
    ImGui::Separator();

    if (ImGui::BeginChild("##BoneTree")) {
        for (const Index_t Root : mRootBones) {
            DrawBoneNode(Root);
        }
    }

    ImGui::EndChild();
}

void FPhysicsAssetEditorToolkit::DrawBoneNode(Index_t Bone) {
    const auto& Bones{mSkeleton->GetBones()};
    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};
    const Core::FString& Name{Bones[Bone].GetName()};
    const Index_t BodyIndex{PhysicsAsset != nullptr ? PhysicsAsset->FindBodyIndex(Name) : InvalidIndex};
    const bool HasBody{BodyIndex != InvalidIndex};

    ImGuiTreeNodeFlags Flags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen};

    if (mBoneChildren[Bone].empty() && !HasBody) {
        Flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (mSelectedBone == Bone && mSelectedConstraint == InvalidIndex && !mSelectedShape.IsValid()) {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(static_cast<int>(Bone));

    const Core::FString Label{HasBody ? Name + "   [Body]" : Name};
    const bool Open{ImGui::TreeNodeEx("##Bone", Flags, "%s", Label.c_str())};

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        mSelectedBone = Bone;
        mSelectedBody = BodyIndex;
        mSelectedShape = PhysicsAssetEditing::FShapeRef{};
        mSelectedConstraint = InvalidIndex;
    }

    if (Open) {
        if (HasBody) {
            const PhysicsRuntime::UBodySetup& Setup{*PhysicsAsset->mSkeletalBodySetups[BodyIndex]};

            // 항목 행을 누르면 그 항목이 선택되어 Details에서 편집할 수 있다.
            const auto DrawShapeRows{[&](PhysicsAssetEditing::EShapeType Type, const char* Name) {
                const Index_t Count{PhysicsAssetEditing::GetShapeCount(Setup, Type)};

                for (Index_t Index{}; Index < Count; ++Index) {
                    ImGuiTreeNodeFlags ShapeFlags{ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth};

                    if (mSelectedBody == BodyIndex && mSelectedShape.mType == Type && mSelectedShape.mIndex == Index) {
                        ShapeFlags |= ImGuiTreeNodeFlags_Selected;
                    }

                    ImGui::PushID(static_cast<int>(Type) * 1000 + static_cast<int>(Index));
                    ImGui::TreeNodeEx("##Shape", ShapeFlags, "%s %zu", Name, Index);

                    if (ImGui::IsItemClicked()) {
                        SelectBody(BodyIndex, PhysicsAssetEditing::FShapeRef{Type, Index});
                    }

                    ImGui::PopID();
                }
            }};

            DrawShapeRows(PhysicsAssetEditing::EShapeType::Sphere, "Sphere");
            DrawShapeRows(PhysicsAssetEditing::EShapeType::Box, "Box");
            DrawShapeRows(PhysicsAssetEditing::EShapeType::Sphyl, "Capsule");
            DrawShapeRows(PhysicsAssetEditing::EShapeType::Convex, "Convex");

            // 이 뼈를 자식으로 삼는 관절 설정을 바디 아래에 보여 준다.
            for (Index_t Index{}; Index < PhysicsAsset->mConstraintSetup.size(); ++Index) {
                const auto& Template{PhysicsAsset->mConstraintSetup[Index]};

                if (Template == nullptr || Template->mDefaultInstance.mConstraintBone1 != Name) {
                    continue;
                }

                ImGuiTreeNodeFlags ConstraintFlags{ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth};

                if (mSelectedConstraint == Index) {
                    ConstraintFlags |= ImGuiTreeNodeFlags_Selected;
                }

                ImGui::PushID(static_cast<int>(Index));
                ImGui::TreeNodeEx("##Constraint", ConstraintFlags, "Constraint -> %s", Template->mDefaultInstance.mConstraintBone2.c_str());

                if (ImGui::IsItemClicked()) {
                    mSelectedBone = Bone;
                    mSelectedBody = BodyIndex;
                    mSelectedShape = PhysicsAssetEditing::FShapeRef{};
                    mSelectedConstraint = Index;
                }

                ImGui::PopID();
            }
        }

        for (const Index_t Child : mBoneChildren[Bone]) {
            DrawBoneNode(Child);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

Asset::USkeleton* FPhysicsAssetEditorToolkit::FindDefaultSkeleton(Core::FString& OutPath) const {
    return PhysicsAssetEditing::FindSkeletonForMesh(*mContext->GetProject()->GetAssets(), mMeshPath, OutPath);
}

void FPhysicsAssetEditorToolkit::DrawSkeletonPicker() {
    const char* Label{mSkeleton == nullptr ? "None" : mSkeletonPath.empty() ? "Embedded Skeleton" : mSkeletonPath.c_str()};

    if (!ImGui::BeginCombo("Skeleton", Label)) {
        return;
    }

    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};
    Core::FString Error{};
    Core::FString SelectedPath{};
    bool Selected{};

    for (const auto& Candidate : Assets->GetAssetPaths(Error)) {
        Asset::EAssetType Type{};

        if (Asset::FAssetFile::GetType(Assets->ResolvePath(Candidate, Error), Type, Error) && Type == Asset::EAssetType::Skeleton) {
            const Core::FString Relative{Core::FilePath::ToGenericUtf8(Candidate)};

            if (ImGui::Selectable(Relative.c_str(), Relative == mSkeletonPath)) {
                SelectedPath = Relative;
                Selected = true;
            }
        }
    }

    ImGui::EndCombo();

    if (!Selected || SelectedPath == mSkeletonPath) {
        return;
    }

    Error.clear();

    Asset::USkeleton* Skeleton{Assets->LoadSkeleton(Core::FilePath::FromUtf8(SelectedPath), Error)};

    if (Skeleton == nullptr) {
        mError = Error.empty() ? "Cannot load the skeleton." : Error;

        return;
    }

    if (BuildPreview(Skeleton)) {
        mSkeletonPath = std::move(SelectedPath);
    }
}

void FPhysicsAssetEditorToolkit::DrawViewport() {
    if (!GetTab().IsVisible()) {
        return;
    }

    if (mContent == nullptr || !mContent->IsReady()) {
        ImGui::TextWrapped("%s", mError.empty() ? "Select a skeleton in Details to preview this mesh." : mError.c_str());

        return;
    }

    if (EditorStyle::Button("Reset View")) {
        mNeedsFrame = true;
    }

    ImGui::SameLine();

    if (EditorStyle::Button(mSimulating ? "Stop" : "Simulate")) {
        if (mSimulating) {
            StopSimulation();
        } else {
            StartSimulation();
        }
    }

    if (mSimulating) {
        ImGui::SameLine();

        if (EditorStyle::Button("Restart")) {
            StartSimulation();
        }
    }

    ImGui::SameLine();

    if (EditorStyle::Button(mDirty ? "Save *" : "Save")) {
        SaveAsset();
    }

    const ImVec2 Size{ImGui::GetContentRegionAvail()};

    if (Size.x < 1.0f || Size.y < 1.0f) {
        return;
    }

    if (!mViewer.Resize(static_cast<Uint32>(Size.x), static_cast<Uint32>(Size.y))) {
        ImGui::TextUnformatted("Cannot resize the physics asset viewport.");

        return;
    }

    ImGui::InvisibleButton("##PhysicsAssetViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    const bool Hovered{ImGui::IsItemHovered()};
    const ImGuiIO& Input{ImGui::GetIO()};
    Viewer::FViewerCamera& Camera{mViewer.GetCamera()};

    if (mNeedsFrame || (Hovered && !Input.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F))) {
        Camera = Viewer::FViewerCamera{};
        mNeedsFrame = !mViewer.Fit(1.15f);
    }

    const bool Flying{ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Right)};

    if (Flying) {
        // 오른쪽 버튼을 누른 동안은 제자리에서 시선을 돌리고 WASD로 이동한다 (E, Q는 위아래).
        Camera.LookAround({Input.MouseDelta.x, Input.MouseDelta.y});

        if (!Input.WantTextInput) {
            const FQuat Rotation{Camera.GetRotation().Quaternion()};
            FVector Move{};

            Move += ImGui::IsKeyDown(ImGuiKey_W) ? Rotation.ForwardVector() : FVector{};
            Move -= ImGui::IsKeyDown(ImGuiKey_S) ? Rotation.ForwardVector() : FVector{};
            Move += ImGui::IsKeyDown(ImGuiKey_D) ? Rotation.RightVector() : FVector{};
            Move -= ImGui::IsKeyDown(ImGuiKey_A) ? Rotation.RightVector() : FVector{};
            Move += ImGui::IsKeyDown(ImGuiKey_E) ? FVector{0.0f, 0.0f, 1.0f} : FVector{};
            Move -= ImGui::IsKeyDown(ImGuiKey_Q) ? FVector{0.0f, 0.0f, 1.0f} : FVector{};

            if (Move.LengthSquared() > 0.0f) {
                const float Speed{Camera.GetRadius() * mCameraSpeed * (Input.KeyShift ? 3.0f : 1.0f)};

                Camera.MoveEye(Move.Normalized() * Speed * Input.DeltaTime);
            }
        }
    } else if (ImGui::IsItemActive()) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            Camera.Pan({Input.MouseDelta.x, Input.MouseDelta.y}, Size.y);
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            Camera.Orbit({Input.MouseDelta.x, Input.MouseDelta.y});
        }
    }

    // 왼쪽 버튼을 거의 움직이지 않고 떼면 클릭으로 보고 그 위치의 바디를 고른다.
    if (!mSimulating && Hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
        const ImVec2 ItemMin{ImGui::GetItemRectMin()};

        PickAt({Input.MousePos.x - ItemMin.x, Input.MousePos.y - ItemMin.y}, Size);
    }

    if (Hovered && Input.MouseWheel != 0.0f) {
        // 오른쪽 버튼을 누른 채 휠을 돌리면 카메라 속도를, 그냥 돌리면 확대와 축소를 조절한다.
        if (Flying) {
            mCameraSpeed = std::clamp(mCameraSpeed * std::pow(1.25f, Input.MouseWheel), 0.1f, 64.0f);
        } else {
            Camera.Zoom(Input.MouseWheel);
        }
    }

    const ImVec2 Origin{ImGui::GetItemRectMin()};
    const ImVec2 End{ImGui::GetItemRectMax()};
    ImDrawList* DrawList{ImGui::GetWindowDrawList()};

    DrawList->AddText({Origin.x + 12.0f, End.y - ImGui::GetTextLineHeight() - 10.0f}, IM_COL32(220, 225, 235, 255), "LMB: orbit | MMB: pan | Wheel: zoom | RMB + WASD/QE: fly | RMB + Wheel: speed | Shift: fast | F: frame");
    DrawList->AddText({Origin.x + 12.0f, Origin.y + 10.0f}, IM_COL32(220, 225, 235, 255), ("Camera speed x" + std::to_string(mCameraSpeed).substr(0, 4)).c_str());

    // 물리는 보이는 탭에서만 고정 스텝 누적기를 통해 진행한다. 긴 프레임은 잘라 낸다.
    if (mSimulating && mContent->GetWorld() != nullptr) {
        mContent->GetWorld()->StepPhysics(std::min(Input.DeltaTime, 0.1f));

        // 월드 Tick이 돌지 않으므로 스텝 결과를 직접 뼈 포즈에 써서 메시가 바디를 따라가게 한다.
        if (Engine::USkeletalMeshComponent* Component{mContent->GetComponent()}) {
            Component->BlendInPhysics();
        }
    }

    Engine::FPrimitiveDrawInterface BodyLines{};

    if (mShowBodies) {
        BuildBodyLines(BodyLines);
    }

    BuildConstraintLines(BodyLines);

    if (!mViewer.Render(&BodyLines)) {
        DrawList->AddText(Origin, ImGui::GetColorU32(ImGuiCol_Text), "Cannot render the physics asset viewport.");

        return;
    }

    const ImTextureRef Texture{static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mViewer.GetNativeTexture()))};
    DrawList->AddImage(Texture, Origin, End);
}

void FPhysicsAssetEditorToolkit::DrawDetails() {
    ImGui::TextUnformatted(Core::FilePath::ToGenericUtf8(GetAssetPath()).c_str());
    ImGui::Separator();

    DrawSkeletonPicker();

    if (!mError.empty()) {
        ImGui::TextWrapped("%s", mError.c_str());
    }

    if (mContent == nullptr || !mContent->IsReady()) {
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Physics Asset");

    const Engine::USkeletalMeshComponent* Component{mContent->GetComponent()};

    if (Component == nullptr) {
        return;
    }

    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{Component->GetPhysicsAsset()};

    if (!mPhysicsAssetPath.empty()) {
        ImGui::Text("Source: File (%s)%s", mPhysicsAssetPath.c_str(), mDirty ? " - unsaved changes" : "");
    } else if (!Component->GetPhysicsAssetOverride().empty()) {
        ImGui::Text("Source: Override (%s)", Component->GetPhysicsAssetOverride().c_str());
    } else if (!mMesh->GetPhysicsAssetPath().empty()) {
        ImGui::Text("Source: Saved (%s)", mMesh->GetPhysicsAssetPath().c_str());
    } else {
        ImGui::TextUnformatted("Source: Generated from the mesh (not saved)");
    }

    // 저장한 파일이 아직 메시의 기본 물리 에셋이 아니면 지정할 수 있다. 저장하지 않은 수정이 있으면 먼저 저장해야 한다.
    if (!mPhysicsAssetPath.empty() && mMesh != nullptr && mMesh->GetPhysicsAssetPath() != mPhysicsAssetPath) {
        ImGui::BeginDisabled(mDirty);

        if (ImGui::Button("Assign To Mesh")) {
            AssignToMesh();
        }

        ImGui::EndDisabled();
    }

    if (PhysicsAsset != nullptr) {
        ImGui::Text("Bodies: %zu", PhysicsAsset->mSkeletalBodySetups.size());
        ImGui::Text("Constraints: %zu", PhysicsAsset->mConstraintSetup.size());
    } else {
        ImGui::TextUnformatted("No physics asset");
    }

    ImGui::Text("Physics scene: %s", mContent->HasPhysicsScene() ? "Created" : "Not created");

    ImGui::Separator();
    ImGui::TextUnformatted("Selection");

    if (mSelectedBone != InvalidIndex && mSkeleton != nullptr) {
        ImGui::Text("Bone: %s", mSkeleton->GetBones()[mSelectedBone].GetName().c_str());
    } else {
        ImGui::TextDisabled("Nothing selected");
    }

    if (PhysicsAsset != nullptr && mSelectedConstraint != InvalidIndex && mSelectedConstraint < PhysicsAsset->mConstraintSetup.size()) {
        const PhysicsRuntime::FConstraintInstance& Instance{PhysicsAsset->mConstraintSetup[mSelectedConstraint]->mDefaultInstance};

        ImGui::Text("Constraint: %s -> %s", Instance.mConstraintBone1.c_str(), Instance.mConstraintBone2.c_str());
    }

    DrawBodyEditor();
    DrawConstraintEditor();

    ImGui::Separator();
    DrawRegenerate();
}

void FPhysicsAssetEditorToolkit::DrawConstraintEditor() {
    PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};
    Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};

    if (PhysicsAsset == nullptr || Component == nullptr || mSkeleton == nullptr) {
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Constraint");

    const bool HasConstraint{mSelectedConstraint != InvalidIndex && mSelectedConstraint < PhysicsAsset->mConstraintSetup.size() && PhysicsAsset->mConstraintSetup[mSelectedConstraint] != nullptr};

    if (!HasConstraint) {
        mFrameRotationConstraint = InvalidIndex;

        const Core::FString BoneName{mSelectedBone != InvalidIndex ? mSkeleton->GetBones()[mSelectedBone].GetName() : Core::FString{}};

        // 바디가 있고 관절이 없는 뼈에는 바디가 있는 가장 가까운 조상과 잇는 관절을 추가할 수 있다.
        if (!BoneName.empty() && PhysicsAsset->FindBodyIndex(BoneName) != InvalidIndex && PhysicsAsset->FindConstraintIndex(BoneName) == InvalidIndex) {
            if (ImGui::Button("Add Constraint To Parent Body")) {
                Component->DestroyPhysicsState();

                const Index_t NewIndex{PhysicsAssetEditing::AddConstraintForBone(*PhysicsAsset, *mSkeleton, mSelectedBone)};

                if (NewIndex != InvalidIndex) {
                    mSelectedConstraint = NewIndex;
                }

                ApplyEdit();
            }
        } else {
            ImGui::TextDisabled("Select a constraint row in Hierarchy.");
        }

        return;
    }

    PhysicsRuntime::FConstraintInstance& Constraint{PhysicsAsset->mConstraintSetup[mSelectedConstraint]->mDefaultInstance};
    bool Changed{};

    ImGui::Text("%s -> %s", Constraint.mConstraintBone1.c_str(), Constraint.mConstraintBone2.c_str());

    if (ImGui::Button("Delete Constraint")) {
        Component->DestroyPhysicsState();
        PhysicsAssetEditing::DeleteConstraint(*PhysicsAsset, mSelectedConstraint);
        mSelectedConstraint = InvalidIndex;
        ApplyEdit();

        return;
    }

    // 회전 축마다 Free/Limited/Locked를 고르고 Limited일 때만 한계 각도를 보여 준다.
    const auto EditAngular{[&Changed](const char* Label, PhysicsRuntime::EAngularConstraintMotion& Motion, float& Limit) {
        int Mode{static_cast<int>(Motion)};

        ImGui::PushID(Label);

        if (ImGui::Combo(Label, &Mode, "Free\0Limited\0Locked\0")) {
            Motion = static_cast<PhysicsRuntime::EAngularConstraintMotion>(Mode);
            Changed = true;
        }

        if (Motion == PhysicsRuntime::EAngularConstraintMotion::Limited && ImGui::DragFloat("Limit (deg)", &Limit, 0.5f, 0.0f, 180.0f, "%.1f")) {
            Changed = true;
        }

        ImGui::PopID();
    }};

    // 이동은 Free와 Locked만 지원한다. Limited는 엔진이 Locked로 취급한다.
    const auto EditLinear{[&Changed](const char* Label, PhysicsRuntime::ELinearConstraintMotion& Motion) {
        int Mode{Motion == PhysicsRuntime::ELinearConstraintMotion::Free ? 0 : 1};

        if (ImGui::Combo(Label, &Mode, "Free\0Locked\0")) {
            Motion = Mode == 0 ? PhysicsRuntime::ELinearConstraintMotion::Free : PhysicsRuntime::ELinearConstraintMotion::Locked;
            Changed = true;
        }
    }};

    // 선택한 관절이 바뀌면 현재 축에서 프레임 회전을 한 번 구해 둔다.
    if (mFrameRotationConstraint != mSelectedConstraint) {
        mFrameRotation[0] = AxesToRotator(Constraint.mPriAxis1, Constraint.mSecAxis1);
        mFrameRotation[1] = AxesToRotator(Constraint.mPriAxis2, Constraint.mSecAxis2);
        mFrameRotationConstraint = mSelectedConstraint;
    }

    const auto EditPosition{[&Changed](const char* Label, FVector& Value) {
        float Fields[3]{Value.x, Value.y, Value.z};

        if (ImGui::DragFloat3(Label, Fields, 0.001f, -100.0f, 100.0f, "%.4f")) {
            Value = FVector{Fields[0], Fields[1], Fields[2]};
            Changed = true;
        }
    }};

    // 회전에서 주 축(X)과 보조 축(Y)을 만들므로 두 축은 항상 직교하는 단위 벡터가 된다.
    const auto EditRotation{[&Changed](FRotator& Rotation, FVector& Primary, FVector& Secondary) {
        float Fields[3]{Rotation.pitch, Rotation.yaw, Rotation.roll};

        if (ImGui::DragFloat3("Rotation (P/Y/R)", Fields, 0.5f, -360.0f, 360.0f, "%.1f")) {
            Rotation = FRotator{Fields[0], Fields[1], Fields[2]};

            const FQuat Quaternion{Rotation.Quaternion()};

            Primary = Quaternion.RotateVector(FVector{1.0f, 0.0f, 0.0f});
            Secondary = Quaternion.RotateVector(FVector{0.0f, 1.0f, 0.0f});
            Changed = true;
        }
    }};

    ImGui::TextUnformatted("Angular");
    EditAngular("Swing 1", Constraint.mSwing1Motion, Constraint.mSwing1LimitDegrees);
    EditAngular("Swing 2", Constraint.mSwing2Motion, Constraint.mSwing2LimitDegrees);
    EditAngular("Twist", Constraint.mTwistMotion, Constraint.mTwistLimitDegrees);

    ImGui::TextUnformatted("Linear");
    EditLinear("Linear X", Constraint.mLinearXMotion);
    EditLinear("Linear Y", Constraint.mLinearYMotion);
    EditLinear("Linear Z", Constraint.mLinearZMotion);

    ImGui::TextUnformatted("Frame 1 (child bone)");
    ImGui::PushID("Frame1");
    EditPosition("Position", Constraint.mPos1);
    EditRotation(mFrameRotation[0], Constraint.mPriAxis1, Constraint.mSecAxis1);
    ImGui::PopID();

    ImGui::TextUnformatted("Frame 2 (parent bone)");
    ImGui::PushID("Frame2");
    EditPosition("Position", Constraint.mPos2);
    EditRotation(mFrameRotation[1], Constraint.mPriAxis2, Constraint.mSecAxis2);
    ImGui::PopID();

    if (Changed) {
        ApplyEdit();
    }
}

void FPhysicsAssetEditorToolkit::BuildConstraintLines(Engine::FPrimitiveDrawInterface& Draw) const {
    const Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};
    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};

    if (Component == nullptr || PhysicsAsset == nullptr || mSelectedConstraint >= PhysicsAsset->mConstraintSetup.size() || PhysicsAsset->mConstraintSetup[mSelectedConstraint] == nullptr) {
        return;
    }

    const PhysicsRuntime::FConstraintInstance& Constraint{PhysicsAsset->mConstraintSetup[mSelectedConstraint]->mDefaultInstance};
    const float AxisLength{0.06f};

    // 프레임 하나를 해당 뼈의 월드 변환에 얹어 X(빨강), Y(초록), Z(파랑) 축으로 그린다.
    const auto DrawFrame{[&](const Core::FString& BoneName, const FVector& Position, const FVector& Primary, const FVector& Secondary, float Brightness) {
        FTransform BoneWorld{};
        const Index_t BoneIndex{Component->GetBoneIndex(BoneName)};

        if (BoneIndex == InvalidIndex || !Component->GetBoneWorldTransform(BoneIndex, BoneWorld)) {
            return;
        }

        const FVector Origin{BoneWorld.TransformPosition(Position)};
        const FVector AxisX{BoneWorld.TransformVectorNoScale(Primary).Normalized()};
        const FVector AxisY{BoneWorld.TransformVectorNoScale(Secondary).Normalized()};
        const FVector AxisZ{AxisX.Cross(AxisY).Normalized()};

        Draw.DrawLine(Origin, Origin + AxisX * AxisLength, FVector{Brightness, 0.1f, 0.1f}, false, 3.0f);
        Draw.DrawLine(Origin, Origin + AxisY * AxisLength, FVector{0.1f, Brightness, 0.1f}, false, 3.0f);
        Draw.DrawLine(Origin, Origin + AxisZ * AxisLength, FVector{0.1f, 0.1f, Brightness}, false, 3.0f);
    }};

    DrawFrame(Constraint.mConstraintBone1, Constraint.mPos1, Constraint.mPriAxis1, Constraint.mSecAxis1, 1.0f);
    DrawFrame(Constraint.mConstraintBone2, Constraint.mPos2, Constraint.mPriAxis2, Constraint.mSecAxis2, 0.55f);
}

void FPhysicsAssetEditorToolkit::SelectBody(Index_t BodyIndex, const PhysicsAssetEditing::FShapeRef& Shape) {
    const Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};
    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};

    mSelectedBody = BodyIndex;
    mSelectedShape = Shape;
    mSelectedConstraint = InvalidIndex;

    if (Component != nullptr && PhysicsAsset != nullptr && BodyIndex < PhysicsAsset->mSkeletalBodySetups.size() && PhysicsAsset->mSkeletalBodySetups[BodyIndex] != nullptr) {
        mSelectedBone = Component->GetBoneIndex(PhysicsAsset->mSkeletalBodySetups[BodyIndex]->GetBoneName());
    }
}

void FPhysicsAssetEditorToolkit::ApplyEdit() {
    Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};

    mDirty = true;

    // 편집하면 시뮬레이션 상태가 무효가 되므로 바닥을 치우고 키네마틱 바디로 다시 만든다.
    mFloorBody.reset();
    mSimulating = false;

    // 바디는 생성 시점의 설명서로 만들어지므로 편집 후에는 물리 상태를 다시 만들어 반영한다.
    if (Component != nullptr) {
        Component->RecreatePhysicsState();
    }
}

bool FPhysicsAssetEditorToolkit::DrawShapeFields(PhysicsRuntime::UBodySetup& Setup, const PhysicsAssetEditing::FShapeRef& Shape) {
    // 항목 크기의 하한이며 단위는 m입니다. 0이면 PhysX 형상 생성이 실패한다.
    constexpr float MinSize{0.005f};

    PhysicsRuntime::FKAggregateGeom& Geometry{Setup.GetAggGeom()};
    bool Changed{};

    const auto EditCenter{[&Changed](FVector& Value) {
        float Fields[3]{Value.x, Value.y, Value.z};

        if (ImGui::DragFloat3("Center", Fields, 0.001f, -100.0f, 100.0f, "%.4f")) {
            Value = FVector{Fields[0], Fields[1], Fields[2]};
            Changed = true;
        }
    }};

    const auto EditRotation{[&Changed](FRotator& Value) {
        float Fields[3]{Value.pitch, Value.yaw, Value.roll};

        if (ImGui::DragFloat3("Rotation (P/Y/R)", Fields, 0.5f, -360.0f, 360.0f, "%.1f")) {
            Value = FRotator{Fields[0], Fields[1], Fields[2]};
            Changed = true;
        }
    }};

    const auto EditSize{[&Changed, MinSize](const char* Label, float& Value) {
        if (ImGui::DragFloat(Label, &Value, 0.001f, MinSize, 100.0f, "%.4f")) {
            Value = std::max(Value, MinSize);
            Changed = true;
        }
    }};

    if (Shape.mIndex >= PhysicsAssetEditing::GetShapeCount(Setup, Shape.mType)) {
        return false;
    }

    switch (Shape.mType) {
        case PhysicsAssetEditing::EShapeType::Sphere: {
            PhysicsRuntime::FKSphereElem& Element{Geometry.mSphereElems[Shape.mIndex]};

            EditCenter(Element.mCenter);
            EditSize("Radius", Element.mRadius);
            break;
        }
        case PhysicsAssetEditing::EShapeType::Box: {
            PhysicsRuntime::FKBoxElem& Element{Geometry.mBoxElems[Shape.mIndex]};
            float Extent[3]{Element.mHalfExtent.x, Element.mHalfExtent.y, Element.mHalfExtent.z};

            EditCenter(Element.mCenter);
            EditRotation(Element.mRotation);

            if (ImGui::DragFloat3("Half Extent", Extent, 0.001f, MinSize, 100.0f, "%.4f")) {
                Element.mHalfExtent = FVector{std::max(Extent[0], MinSize), std::max(Extent[1], MinSize), std::max(Extent[2], MinSize)};
                Changed = true;
            }

            break;
        }
        case PhysicsAssetEditing::EShapeType::Sphyl: {
            PhysicsRuntime::FKSphylElem& Element{Geometry.mSphylElems[Shape.mIndex]};

            EditCenter(Element.mCenter);
            EditRotation(Element.mRotation);
            EditSize("Radius", Element.mRadius);
            EditSize("Length", Element.mLength);
            break;
        }
        default:
            ImGui::TextDisabled("Convex shapes cannot be edited here.");
            break;
    }

    return Changed;
}

void FPhysicsAssetEditorToolkit::DrawBodyEditor() {
    PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};
    Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};

    if (PhysicsAsset == nullptr || Component == nullptr || mSkeleton == nullptr) {
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Body");

    const bool HasBody{mSelectedBody != InvalidIndex && mSelectedBody < PhysicsAsset->mSkeletalBodySetups.size() && PhysicsAsset->mSkeletalBodySetups[mSelectedBody] != nullptr};

    if (!HasBody) {
        if (mSelectedBone == InvalidIndex) {
            ImGui::TextDisabled("Select a bone or a body.");

            return;
        }

        // 선택한 뼈에 바디가 없으면 정점 분포에 맞춘 기본 캡슐 바디를 추가할 수 있다.
        if (ImGui::Button("Add Body To Bone")) {
            Component->DestroyPhysicsState();

            const Index_t NewBody{PhysicsAssetEditing::AddBodyForBone(*PhysicsAsset, *mMesh, *mSkeleton, mSelectedBone)};

            if (NewBody != InvalidIndex) {
                SelectBody(NewBody, PhysicsAssetEditing::FShapeRef{});
            }

            ApplyEdit();
        }

        return;
    }

    PhysicsRuntime::UBodySetup& Setup{*PhysicsAsset->mSkeletalBodySetups[mSelectedBody]};

    ImGui::Text("Body %zu (%s): %zu shapes", mSelectedBody, Setup.GetBoneName().c_str(), PhysicsAssetEditing::GetTotalShapeCount(Setup));

    if (ImGui::Button("Delete Body")) {
        // 바디를 지우면 그 뼈를 잇던 관절 설정도 함께 지워지고 번호가 당겨지므로 선택을 해제한다.
        Component->DestroyPhysicsState();
        Asset::FPhysicsAssetUtils::DestroyBody(*PhysicsAsset, mSelectedBody);
        mSelectedBody = InvalidIndex;
        mSelectedShape = PhysicsAssetEditing::FShapeRef{};
        mSelectedConstraint = InvalidIndex;
        ApplyEdit();

        return;
    }

    ImGui::TextUnformatted("Add Shape");

    for (const auto& [Label, Type] : {std::pair{"Sphere", PhysicsAssetEditing::EShapeType::Sphere}, std::pair{"Box", PhysicsAssetEditing::EShapeType::Box}, std::pair{"Capsule", PhysicsAssetEditing::EShapeType::Sphyl}}) {
        ImGui::SameLine();

        if (ImGui::Button(Label)) {
            Component->DestroyPhysicsState();
            mSelectedShape = PhysicsAssetEditing::AddShape(Setup, Type);
            ApplyEdit();
        }
    }

    if (!mSelectedShape.IsValid()) {
        ImGui::TextDisabled("Select a shape in Hierarchy or click one in the viewport.");

        return;
    }

    ImGui::Separator();
    ImGui::Text("Shape: %s %zu", mSelectedShape.mType == PhysicsAssetEditing::EShapeType::Sphere ? "Sphere" : mSelectedShape.mType == PhysicsAssetEditing::EShapeType::Box ? "Box" : mSelectedShape.mType == PhysicsAssetEditing::EShapeType::Sphyl ? "Capsule" : "Convex", mSelectedShape.mIndex);

    if (DrawShapeFields(Setup, mSelectedShape)) {
        ApplyEdit();
    }

    // 항목이 하나도 없는 바디는 PhysX Actor를 만들 수 없으므로 마지막 항목은 지울 수 없다.
    ImGui::BeginDisabled(PhysicsAssetEditing::GetTotalShapeCount(Setup) <= 1);

    if (ImGui::Button("Delete Shape")) {
        Component->DestroyPhysicsState();
        PhysicsAssetEditing::DeleteShape(Setup, mSelectedShape);
        mSelectedShape = PhysicsAssetEditing::FShapeRef{};
        ApplyEdit();
    }

    ImGui::EndDisabled();
}

void FPhysicsAssetEditorToolkit::DrawRegenerate() {
    PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};

    if (PhysicsAsset == nullptr || mSkeleton == nullptr) {
        return;
    }

    ImGui::TextUnformatted("Regenerate (auto fit)");

    constexpr const char* GeomNames[]{"Box", "Capsule", "Sphere"};
    constexpr Asset::EPhysAssetFitGeomType GeomTypes[]{Asset::EPhysAssetFitGeomType::Box, Asset::EPhysAssetFitGeomType::Sphyl, Asset::EPhysAssetFitGeomType::Sphere};
    int Geom{};

    for (int Index{}; Index < 3; ++Index) {
        if (GeomTypes[Index] == mRegenParams.mGeomType) {
            Geom = Index;
        }
    }

    if (ImGui::Combo("Primitive", &Geom, GeomNames, 3)) {
        mRegenParams.mGeomType = GeomTypes[Geom];
    }

    int Weight{mRegenParams.mVertWeight == Asset::EPhysAssetFitVertWeight::AnyWeight ? 0 : 1};

    if (ImGui::Combo("Vertex Weight", &Weight, "Any\0Dominant\0")) {
        mRegenParams.mVertWeight = Weight == 0 ? Asset::EPhysAssetFitVertWeight::AnyWeight : Asset::EPhysAssetFitVertWeight::DominantWeight;
    }

    ImGui::DragFloat("Min Bone Size", &mRegenParams.mMinBoneSize, 0.005f, 0.0f, 10.0f, "%.3f");
    ImGui::Checkbox("Create Constraints", &mRegenParams.mCreateConstraints);
    ImGui::Checkbox("Body For All Bones", &mRegenParams.mBodyForAll);

    int Angular{static_cast<int>(mRegenParams.mAngularConstraintMode)};

    if (ImGui::Combo("Angular Motion", &Angular, "Free\0Limited\0Locked\0")) {
        mRegenParams.mAngularConstraintMode = static_cast<PhysicsRuntime::EAngularConstraintMotion>(Angular);
    }

    if (ImGui::Button("Regenerate...")) {
        mOpenRegeneratePopup = true;
    }

    if (mOpenRegeneratePopup) {
        ImGui::OpenPopup("Regenerate Physics Asset");
        mOpenRegeneratePopup = false;
    }

    if (ImGui::BeginPopupModal("Regenerate Physics Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("All bodies, shapes and constraints will be replaced.");
        ImGui::TextUnformatted("This cannot be undone.");

        if (ImGui::Button("Regenerate")) {
            Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};

            if (Component != nullptr) {
                Component->DestroyPhysicsState();
            }

            if (!Asset::FPhysicsAssetUtils::CreateFromSkeletalMesh(*PhysicsAsset, *mMesh, *mSkeleton, mRegenParams)) {
                mError = "Cannot regenerate the physics asset.";
            }

            mSelectedBody = InvalidIndex;
            mSelectedShape = PhysicsAssetEditing::FShapeRef{};
            mSelectedConstraint = InvalidIndex;
            ApplyEdit();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FPhysicsAssetEditorToolkit::PickAt(const ImVec2& Mouse, const ImVec2& Size) {
    Engine::UWorld* World{mContent != nullptr ? mContent->GetWorld() : nullptr};
    PhysicsRuntime::FPhysicsScene* Scene{World != nullptr ? World->GetPhysicsScene() : nullptr};

    if (Scene == nullptr) {
        return;
    }

    FRay Ray{};
    const Engine::FSceneView View{mViewer.GetCamera().CalcSceneView({Size.x, Size.y})};

    if (!View.Deproject({Mouse.x, Mouse.y}, Ray)) {
        return;
    }

    Index_t BodyIndex{InvalidIndex};
    Index_t ShapeOrdinal{InvalidIndex};

    if (!PhysicsAssetEditing::PickBody(*Scene, Ray, 1000.0f, BodyIndex, ShapeOrdinal)) {
        // 빈 곳을 누르면 선택을 해제한다.
        mSelectedBody = InvalidIndex;
        mSelectedBone = InvalidIndex;
        mSelectedShape = PhysicsAssetEditing::FShapeRef{};
        mSelectedConstraint = InvalidIndex;

        return;
    }

    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{GetPhysicsAsset()};
    PhysicsAssetEditing::FShapeRef Shape{};

    if (PhysicsAsset != nullptr && BodyIndex < PhysicsAsset->mSkeletalBodySetups.size() && PhysicsAsset->mSkeletalBodySetups[BodyIndex] != nullptr && ShapeOrdinal != InvalidIndex) {
        Shape = PhysicsAssetEditing::FindShapeByOrdinal(*PhysicsAsset->mSkeletalBodySetups[BodyIndex], ShapeOrdinal);
    }

    SelectBody(BodyIndex, Shape);
}
