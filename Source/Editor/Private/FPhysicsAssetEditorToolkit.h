/**
 * @file FPhysicsAssetEditorToolkit.h
 * @brief 스켈레탈 메시의 물리 에셋을 미리보기와 함께 편집하는 에셋 툴킷을 정의합니다.
 * @ingroup Editor
 */

#pragma once

#include "FAssetEditorToolkit.h"
#include "Viewer/FAssetViewer.h"

#include <filesystem>

namespace Asset {
    class USkeletalMesh;
    class USkeleton;
}

namespace Engine {
    class FPrimitiveDrawInterface;
}

namespace PhysicsRuntime {
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
         * @param[in] MeshPath 편집할 스켈레탈 메시의 콘텐츠 경로입니다.
         * @param[in] Mesh 호출자가 수명을 보장하는 스켈레탈 메시입니다.
         * @param[in] Skeleton 메시와 호환되는 스켈레톤이며 nullptr이면 상세 패널에서 고릅니다.
         */
        FPhysicsAssetEditorToolkit(Editor::FEditorContext* Context, const std::filesystem::path& MeshPath, Asset::USkeletalMesh* Mesh, Asset::USkeleton* Skeleton);

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

    private:

        /**
         * @brief 편집 대상과 공유 서비스에 접근하는 에디터 컨텍스트의 비소유 주소입니다.
         */
        Editor::FEditorContext* mContext{};

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
         * @brief 뷰포트에 바디 형상을 그릴지 나타냅니다.
         */
        bool mShowBodies{true};
    };

}
