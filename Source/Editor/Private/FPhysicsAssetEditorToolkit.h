/**
 * @file FPhysicsAssetEditorToolkit.h
 * @brief 스켈레탈 메시의 물리 에셋을 미리보기와 함께 편집하는 에셋 툴킷을 정의합니다.
 * @ingroup Editor
 */

#pragma once

#include "FAssetEditorToolkit.h"
#include "FPhysicsAssetEditing.h"
#include "Asset/FPhysicsAssetUtils.h"
#include "Viewer/FAssetViewer.h"

#include "Core/Math/FRotator.h"

#include <filesystem>
#include <memory>

namespace Asset {
    class USkeletalMesh;
    class USkeleton;
}

struct ImVec2;

namespace Engine {
    class FPrimitiveDrawInterface;
}

namespace PhysicsRuntime {
    class FBodyInstance;
    class UBodySetup;
    class UPhysicsAsset;
}

namespace Editor {
    class FEditorContext;
    class FPhysicsAssetViewerContent;
}

namespace Editor {

    /**
     * @brief 스켈레탈 메시의 물리 에셋을 미리보기와 함께 편집하는 에셋 툴킷입니다.
     * @details 편집 대상은 메시가 가리키는 물리 에셋이며, 지정된 에셋이 없으면 메시와 뼈대로 자동 생성한 에셋입니다.
     *          에셋 경로는 편집 중인 스켈레탈 메시의 경로입니다.
     * @ingroup Editor
     */
    class FPhysicsAssetEditorToolkit final : public Editor::FAssetEditorToolkit {
    public:

        /**
         * @brief 메시 미리보기와 상세 패널을 가진 툴킷을 구성합니다.
         * @param[in] Context 편집 대상과 공유 서비스를 제공하는 에디터 컨텍스트의 비소유 주소입니다.
         * @param[in] AssetPath 탭을 식별하는 경로입니다. 물리 에셋 파일을 열면 그 경로이고, 메시에서 열면 메시 경로입니다.
         * @param[in] MeshPath 미리보기에 쓰는 스켈레탈 메시의 콘텐츠 경로입니다.
         * @param[in] Mesh 호출자가 수명을 보장하는 스켈레탈 메시입니다.
         * @param[in] Skeleton 메시와 호환되는 스켈레톤이며 nullptr이면 상세 패널에서 고릅니다.
         * @param[in] PhysicsAssetPath 편집할 물리 에셋 파일의 콘텐츠 경로이며, 비어 있으면 메시가 가리키거나 자동 생성한 에셋을 편집합니다.
         */
        FPhysicsAssetEditorToolkit(Editor::FEditorContext* Context, const std::filesystem::path& AssetPath, const std::filesystem::path& MeshPath, Asset::USkeletalMesh* Mesh, Asset::USkeleton* Skeleton, const Core::FString& PhysicsAssetPath);

        /**
         * @brief 미리보기와 소유 멤버를 정리합니다.
         */
        ~FPhysicsAssetEditorToolkit() override;

    private:

        /**
         * @brief 탭 제목을 갱신합니다.
         * @param[in] DeltaTime 이전 프레임 이후 경과한 시간이며 단위는 초입니다.
         */
        void UpdateEditor(float DeltaTime) override;

        /**
         * @brief 지정한 스켈레톤으로 미리보기 콘텐츠를 새로 만듭니다.
         * @param[in] Skeleton 미리보기에 사용할 스켈레톤입니다.
         * @return 미리보기를 준비했으면 true이며 실패 원인은 mError에 기록합니다.
         */
        bool BuildPreview(Asset::USkeleton* Skeleton);

        /**
         * @brief 메시에 내장된 스켈레톤이나 같은 이름 규칙의 스켈레톤 에셋을 찾습니다.
         * @param[out] OutPath 스켈레톤 에셋을 찾았으면 그 콘텐츠 경로이고, 내장 스켈레톤이면 비어 있습니다.
         * @return 찾은 스켈레톤의 비소유 주소이며 없으면 nullptr입니다.
         */
        Asset::USkeleton* FindDefaultSkeleton(Core::FString& OutPath) const;

        /**
         * @brief 미리보기 이미지를 표시하고 카메라 입력을 처리합니다.
         */
        void DrawViewport();

        /**
         * @brief 메시, 스켈레톤과 물리 에셋의 상태를 표시합니다.
         */
        void DrawDetails();

        /**
         * @brief 스켈레톤 에셋을 고르는 콤보 상자를 표시합니다.
         */
        void DrawSkeletonPicker();

        /**
         * @brief 뼈 계층, 바디와 관절 항목을 트리로 표시하고 선택을 처리합니다.
         */
        void DrawHierarchy();

        /**
         * @brief 뼈 하나와 그 자식 뼈, 뼈에 붙은 바디의 항목을 재귀로 표시합니다.
         * @param[in] Bone 표시할 뼈의 번호입니다.
         */
        void DrawBoneNode(Index_t Bone);

        /**
         * @brief 스켈레톤의 부모·자식 관계로 트리 표시용 목록을 다시 만들고 선택을 지웁니다.
         */
        void RebuildBoneTree();

        /**
         * @brief 미리보기 컴포넌트가 사용하는 물리 에셋을 반환합니다.
         * @return 비소유 주소이며 미리보기가 없으면 nullptr입니다.
         */
        PhysicsRuntime::UPhysicsAsset* GetPhysicsAsset() const;

        /**
         * @brief 물리 에셋의 모든 바디를 뼈의 현재 월드 변환 기준 선으로 기록합니다.
         * @param[in,out] Draw 선을 기록할 그리기 인터페이스입니다.
         */
        void BuildBodyLines(Engine::FPrimitiveDrawInterface& Draw) const;

        /**
         * @brief 바디(와 선택적으로 항목)를 선택하고 그 뼈도 함께 선택합니다.
         * @param[in] BodyIndex 선택할 바디 설명서의 번호입니다.
         * @param[in] Shape 선택할 항목이며 유효하지 않으면 바디만 선택합니다.
         */
        void SelectBody(Index_t BodyIndex, const PhysicsAssetEditing::FShapeRef& Shape);

        /**
         * @brief 편집 결과를 미리보기의 물리 상태에 반영하고 수정 표시를 남깁니다.
         */
        void ApplyEdit();

        /**
         * @brief 선택한 바디의 추가·삭제 버튼과 선택한 항목의 편집 필드를 표시합니다.
         */
        void DrawBodyEditor();

        /**
         * @brief 항목 하나의 위치, 회전, 크기 필드를 표시하고 값이 바뀌면 true를 반환합니다.
         * @param[in,out] Setup 편집할 바디 설명서입니다.
         * @param[in] Shape 편집할 항목입니다.
         * @return 값이 바뀌었으면 true입니다.
         */
        bool DrawShapeFields(PhysicsRuntime::UBodySetup& Setup, const PhysicsAssetEditing::FShapeRef& Shape);

        /**
         * @brief 편집 중인 물리 에셋을 파일에 저장합니다.
         * @details 아직 파일이 없으면 "<메시 이름>_PhysicsAsset" 경로에 저장하고 이후 그 파일을 편집 대상으로 삼습니다.
         */
        void SaveAsset();

        /**
         * @brief 저장한 물리 에셋을 미리보기 메시의 기본 물리 에셋으로 지정하고 메시 파일을 저장합니다.
         * @note 본을 내장한 레거시 메시는 저장하면 본이 사라질 수 있어 거부합니다.
         */
        void AssignToMesh();

        /**
         * @brief 바닥을 만들고 모든 뼈 바디를 시뮬레이션으로 바꿔 물리를 시작합니다.
         * @details 시작 전에 물리 상태를 다시 만들어 애니메이션 자세에서 출발합니다.
         */
        void StartSimulation();

        /**
         * @brief 바닥을 제거하고 물리 상태를 다시 만들어 시뮬레이션 전 자세로 되돌립니다.
         */
        void StopSimulation();

        /**
         * @brief 선택한 관절의 움직임 방식, 한계 각도와 프레임 필드를 표시하고 관절을 추가·삭제합니다.
         */
        void DrawConstraintEditor();

        /**
         * @brief 선택한 관절의 두 프레임(위치와 세 축)을 뼈의 현재 월드 변환 기준 선으로 기록합니다.
         * @param[in,out] Draw 선을 기록할 그리기 인터페이스입니다.
         */
        void BuildConstraintLines(Engine::FPrimitiveDrawInterface& Draw) const;

        /**
         * @brief 모든 바디를 자동 생성으로 다시 만드는 설정과 확인 창을 표시합니다.
         */
        void DrawRegenerate();

        /**
         * @brief 뷰포트를 클릭한 위치에서 PhysX 광선을 쏘아 바디를 선택합니다.
         * @param[in] Mouse 뷰포트 이미지 왼쪽 위 기준의 마우스 위치(픽셀)입니다.
         * @param[in] Size 뷰포트 이미지 크기(픽셀)입니다.
         */
        void PickAt(const ImVec2& Mouse, const ImVec2& Size);

    private:

        /**
         * @brief 편집 대상과 공유 서비스에 접근하는 에디터 컨텍스트의 비소유 주소입니다.
         */
        Editor::FEditorContext* mContext{};

        /**
         * @brief 미리보기에 쓰는 스켈레탈 메시의 콘텐츠 경로입니다.
         */
        std::filesystem::path mMeshPath{};

        /**
         * @brief 편집 중인 물리 에셋 파일의 콘텐츠 경로이며, 파일이 없으면 비어 있습니다.
         */
        Core::FString mPhysicsAssetPath{};

        /**
         * @brief 편집 중인 스켈레탈 메시의 비소유 주소입니다.
         */
        Asset::USkeletalMesh* mMesh{};

        /**
         * @brief 미리보기에 사용 중인 스켈레톤의 비소유 주소입니다.
         */
        Asset::USkeleton* mSkeleton{};

        /**
         * @brief 사용 중인 스켈레톤 에셋의 콘텐츠 경로이며 메시에 내장된 스켈레톤이면 비어 있습니다.
         */
        Core::FString mSkeletonPath{};

        /**
         * @brief 미리보기 카메라와 렌더 타깃을 관리하는 공통 뷰어입니다.
         */
        Viewer::FAssetViewer mViewer;

        /**
         * @brief 뷰어가 소유하는 미리보기 콘텐츠의 비소유 주소입니다.
         */
        Editor::FPhysicsAssetViewerContent* mContent{};

        /**
         * @brief 마지막 작업의 오류 메시지입니다.
         */
        Core::FString mError{};

        /**
         * @brief 다음 그리기에서 카메라를 메시에 맞춰야 하는지 나타냅니다.
         */
        bool mNeedsFrame{true};

        /**
         * @brief 오른쪽 버튼을 누른 채 이동할 때의 카메라 속도이며 단위는 메시 반지름/초입니다.
         */
        float mCameraSpeed{1.0f};

        /**
         * @brief 스켈레톤 뼈마다 자식 뼈 번호를 담은 트리 표시용 목록입니다.
         */
        Core::TArray<Core::TArray<Index_t>> mBoneChildren{};

        /**
         * @brief 부모가 없는 뼈의 번호 목록입니다.
         */
        Core::TArray<Index_t> mRootBones{};

        /**
         * @brief 선택한 뼈의 번호이며 없으면 InvalidIndex입니다.
         */
        Index_t mSelectedBone{InvalidIndex};

        /**
         * @brief 선택한 바디 설명서의 번호(물리 에셋 안의 순서)이며 없으면 InvalidIndex입니다.
         */
        Index_t mSelectedBody{InvalidIndex};

        /**
         * @brief 선택한 관절 설정의 번호이며 없으면 InvalidIndex입니다.
         */
        Index_t mSelectedConstraint{InvalidIndex};

        /**
         * @brief 선택한 바디 안의 항목이며 항목을 고르지 않았으면 유효하지 않은 값입니다.
         */
        PhysicsAssetEditing::FShapeRef mSelectedShape{};

        /**
         * @brief 관절 프레임의 회전 필드가 가리키는 관절 번호이며, 다시 계산해야 하면 InvalidIndex입니다.
         */
        Index_t mFrameRotationConstraint{InvalidIndex};

        /**
         * @brief 편집 중인 관절의 Frame 1, Frame 2 회전(도)입니다.
         * @details 축에서 오일러 각을 매 프레임 다시 구하면 드래그 중 값이 튀므로 선택할 때 한 번만 구해 둡니다.
         */
        FRotator mFrameRotation[2]{};

        /**
         * @brief 에셋을 수정했지만 저장하지 않았는지 나타냅니다.
         */
        bool mDirty{};

        /**
         * @brief 자동 생성 다시 만들기에 쓸 설정입니다.
         */
        Asset::FPhysAssetCreateParams mRegenParams{};

        /**
         * @brief 다시 만들기 확인 창을 열어야 하는지 나타냅니다.
         */
        bool mOpenRegeneratePopup{};

        /**
         * @brief 뷰포트에 바디 형상을 그릴지 나타냅니다.
         */
        bool mShowBodies{true};

        /**
         * @brief 미리보기에서 물리를 진행 중인지 나타냅니다.
         */
        bool mSimulating{};

        /**
         * @brief 시뮬레이션을 시작할 때의 메시 월드 범위이며 바닥 위치를 정합니다.
         * @note 시뮬레이션 중 메시가 움직여도 바닥이 따라 움직이지 않도록 시작 시점 값을 고정합니다.
         */
        FBox mFloorBounds{};

        /**
         * @brief 시뮬레이션 중에만 존재하는 바닥 바디입니다.
         * @note 물리 장면보다 먼저 해제되도록 가장 마지막에 선언합니다.
         */
        std::unique_ptr<PhysicsRuntime::FBodyInstance> mFloorBody{};
    };

}
