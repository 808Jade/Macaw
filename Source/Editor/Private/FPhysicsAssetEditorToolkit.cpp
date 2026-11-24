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
#include "Editor/EEditorIcon.h"
#include "Editor/FEditorContext.h"
#include "Editor/FEditorToolkitHost.h"
#include "Engine/Component/USkeletalMeshComponent.h"
#include "Engine/FPrimitiveDrawInterface.h"
#include "Engine/FProject.h"
#include "Engine/UWorld.h"
#include "PhysicsRuntime/UBodySetup.h"
#include "PhysicsRuntime/UPhysicsAsset.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>

using namespace Editor;

FPhysicsAssetEditorToolkit::FPhysicsAssetEditorToolkit(FEditorContext* Context, const std::filesystem::path& MeshPath, Asset::USkeletalMesh* Mesh, Asset::USkeleton* Skeleton)
    : FAssetEditorToolkit{MeshPath, Asset::EAssetType::PhysicsAsset, "###PhysicsAssetEditor:" + Core::FilePath::ToGenericUtf8(MeshPath)},
      mContext{Context},
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
    GetTab().SetName(EditorStyle::TabIconPadding + Core::FilePath::ToUtf8(GetAssetPath().stem()) + " Physics" + "###PhysicsAssetEditor:" + Core::FilePath::ToGenericUtf8(GetAssetPath()));
}

bool FPhysicsAssetEditorToolkit::BuildPreview(Asset::USkeleton* Skeleton) {
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

    return true;
}

void FPhysicsAssetEditorToolkit::RebuildBoneTree() {
    mBoneChildren.clear();
    mRootBones.clear();
    mSelectedBone = InvalidIndex;
    mSelectedBody = InvalidIndex;
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

void FPhysicsAssetEditorToolkit::BuildBodyLines(Engine::FPrimitiveDrawInterface& Draw) const {
    const Engine::USkeletalMeshComponent* Component{mContent != nullptr ? mContent->GetComponent() : nullptr};
    const PhysicsRuntime::UPhysicsAsset* PhysicsAsset{Component != nullptr ? Component->GetPhysicsAsset() : nullptr};

    if (PhysicsAsset == nullptr) {
        return;
    }

    for (Index_t Index{}; Index < PhysicsAsset->mSkeletalBodySetups.size(); ++Index) {
        const PhysicsRuntime::UBodySetup* Setup{PhysicsAsset->mSkeletalBodySetups[Index].get()};
        FTransform BoneWorld{};

        if (Setup == nullptr) {
            continue;
        }

        // 바디를 만들 때와 같은 뼈 월드 변환(스케일 포함)을 써서 실제 충돌 모양과 같은 크기로 그린다.
        const Index_t BoneIndex{Component->GetBoneIndex(Setup->GetBoneName())};

        if (BoneIndex == InvalidIndex || !Component->GetBoneWorldTransform(BoneIndex, BoneWorld)) {
            continue;
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

    if (mSelectedBone == Bone && mSelectedConstraint == InvalidIndex) {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(static_cast<int>(Bone));

    const Core::FString Label{HasBody ? Name + "   [Body]" : Name};
    const bool Open{ImGui::TreeNodeEx("##Bone", Flags, "%s", Label.c_str())};

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        mSelectedBone = Bone;
        mSelectedBody = BodyIndex;
        mSelectedConstraint = InvalidIndex;
    }

    if (Open) {
        if (HasBody) {
            const PhysicsRuntime::FKAggregateGeom& Geometry{PhysicsAsset->mSkeletalBodySetups[BodyIndex]->GetAggGeom()};

            for (Index_t Index{}; Index < Geometry.mSphereElems.size(); ++Index) {
                ImGui::BulletText("Sphere %zu", Index);
            }

            for (Index_t Index{}; Index < Geometry.mBoxElems.size(); ++Index) {
                ImGui::BulletText("Box %zu", Index);
            }

            for (Index_t Index{}; Index < Geometry.mSphylElems.size(); ++Index) {
                ImGui::BulletText("Capsule %zu", Index);
            }

            for (Index_t Index{}; Index < Geometry.mConvexElems.size(); ++Index) {
                ImGui::BulletText("Convex %zu", Index);
            }

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
    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};

    OutPath.clear();

    if (Asset::USkeleton* Legacy{Assets->FindLegacySkeleton(GetAssetPath())}) {
        return Legacy;
    }

    // 임포트가 만드는 "<메시 이름>_Skeleton" 에셋이 같은 폴더에 있으면 그것을 기본값으로 쓴다.
    std::filesystem::path Sibling{GetAssetPath()};
    Sibling.replace_filename(Sibling.stem().native() + L"_Skeleton" + Sibling.extension().native());

    Core::FString Error{};
    Asset::USkeleton* Skeleton{Assets->LoadSkeleton(Sibling, Error)};

    if (Skeleton != nullptr) {
        OutPath = Core::FilePath::ToGenericUtf8(Sibling);
    }

    return Skeleton;
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

    Engine::FPrimitiveDrawInterface BodyLines{};

    if (mShowBodies) {
        BuildBodyLines(BodyLines);
    }

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

    if (!Component->GetPhysicsAssetOverride().empty()) {
        ImGui::Text("Source: Override (%s)", Component->GetPhysicsAssetOverride().c_str());
    } else if (!mMesh->GetPhysicsAssetPath().empty()) {
        ImGui::Text("Source: Saved (%s)", mMesh->GetPhysicsAssetPath().c_str());
    } else {
        ImGui::TextUnformatted("Source: Generated from the mesh (not saved)");
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

    if (PhysicsAsset != nullptr && mSelectedBody != InvalidIndex && mSelectedBody < PhysicsAsset->mSkeletalBodySetups.size()) {
        const PhysicsRuntime::FKAggregateGeom& Geometry{PhysicsAsset->mSkeletalBodySetups[mSelectedBody]->GetAggGeom()};

        ImGui::Text("Body %zu: %zu sphere, %zu box, %zu capsule, %zu convex", mSelectedBody, Geometry.mSphereElems.size(), Geometry.mBoxElems.size(), Geometry.mSphylElems.size(), Geometry.mConvexElems.size());
    }

    if (PhysicsAsset != nullptr && mSelectedConstraint != InvalidIndex && mSelectedConstraint < PhysicsAsset->mConstraintSetup.size()) {
        const PhysicsRuntime::FConstraintInstance& Instance{PhysicsAsset->mConstraintSetup[mSelectedConstraint]->mDefaultInstance};

        ImGui::Text("Constraint: %s -> %s", Instance.mConstraintBone1.c_str(), Instance.mConstraintBone2.c_str());
    }
}
