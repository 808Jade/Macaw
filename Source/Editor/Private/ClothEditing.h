/**
 * @file ClothEditing.h
 * @brief 에디터 명령과 메시 뷰어가 함께 쓰는 천 에셋 만들기, 빼기, 설정 목록을 선언합니다.
 * @ingroup Editor
 */

#pragma once

#include "Core/Types.h"
#include "Core/Math/FVector.h"

#include <utility>

namespace Asset {
    class USkeletalMesh;
    class USkeleton;
}

namespace PhysicsRuntime {
    class UClothConfigNv;
    class UClothingAssetCommon;
    class UPhysicsAsset;
    enum class EWeightMapTargetCommon : Uint8;
}

namespace Editor {

    /**
     * @brief 에디터 명령과 메시 뷰어가 함께 쓰는 천 편집 함수 모음입니다.
     * @ingroup Editor
     */
    namespace ClothEditing {
        /**
         * @brief 스켈레톤의 뼈 이름을 뼈 번호 순서로 모읍니다.
         * @param[in] Skeleton 뼈를 가진 스켈레톤입니다.
         * @return 뼈 번호 순서의 뼈 이름입니다.
         */
        Core::TArray<Core::FString> GetSkeletonBoneNames(const Asset::USkeleton& Skeleton);

        /**
         * @brief 섹션 하나로 천 에셋을 만들어 메시에 넣고 섹션에 연결합니다.
         * @details 최대 거리를 칠하기 전에도 움직이도록 위쪽 일부 입자만 고정하는 초기값을 넣습니다.
         * @param[in,out] Mesh 섹션을 가진 메시입니다.
         * @param[in] Skeleton 메시가 쓰는 스켈레톤입니다.
         * @param[in] SubMeshIndex 천으로 바꿀 섹션 번호입니다.
         * @param[in] PhysicsAsset 천이 부딪힐 몸을 담은 물리 에셋이며 nullptr이면 부딪히지 않습니다.
         * @param[in] PhysicsAssetPath 물리 에셋의 Content 상대 경로이며 저장하지 않은 물리 에셋이면 비워 둡니다.
         * @param[out] Error 실패 원인입니다.
         * @return 만들고 연결한 천 에셋이며 실패하면 nullptr입니다.
         */
        PhysicsRuntime::UClothingAssetCommon* CreateClothFromSection(Asset::USkeletalMesh& Mesh, const Asset::USkeleton& Skeleton, Index_t SubMeshIndex, PhysicsRuntime::UPhysicsAsset* PhysicsAsset, const Core::FString& PhysicsAssetPath, Core::FString& Error);

        /**
         * @brief 설정의 실수 항목 이름과 값 위치를 모읍니다.
         * @param[in] Config 값을 가진 설정입니다.
         * @return 소문자 이름과 값 위치입니다.
         */
        Core::TArray<std::pair<Core::FString, float*>> GetConfigFloats(PhysicsRuntime::UClothConfigNv& Config);

        /**
         * @brief 설정의 벡터 항목 이름과 값 위치를 모읍니다.
         * @param[in] Config 값을 가진 설정입니다.
         * @return 소문자 이름과 값 위치입니다.
         */
        Core::TArray<std::pair<Core::FString, FVector*>> GetConfigVectors(PhysicsRuntime::UClothConfigNv& Config);

        /**
         * @brief 칠할 수 있는 가중치 맵 대상의 수입니다.
         */
        inline constexpr int NumPaintTargets{4};

        /**
         * @brief 칠하기 대상 번호의 표시 이름을 반환합니다.
         * @param[in] TargetIndex 0부터 NumPaintTargets - 1 사이의 대상 번호입니다.
         * @return 표시 이름입니다.
         */
        const char* GetPaintTargetName(int TargetIndex);

        /**
         * @brief 칠하기 대상 번호를 가중치 맵 대상으로 바꿉니다.
         * @param[in] TargetIndex 0부터 NumPaintTargets - 1 사이의 대상 번호입니다.
         * @return 가중치 맵 대상입니다.
         */
        PhysicsRuntime::EWeightMapTargetCommon GetPaintTarget(int TargetIndex);

        /**
         * @brief 칠한 가중치 맵을 천 에셋에 반영합니다.
         * @details 편집용 가중치 맵을 맞추고 역질량, 천끼리 부딪힐 입자, 섹션 정점의 스키닝만 쓰는 표시를 다시 구합니다.
         * @param[in,out] Mesh 천 에셋을 가진 메시입니다.
         * @param[in,out] ClothingAsset 칠한 천 에셋입니다.
         */
        void ApplyPaintedWeights(Asset::USkeletalMesh& Mesh, PhysicsRuntime::UClothingAssetCommon& ClothingAsset);
    }
}
