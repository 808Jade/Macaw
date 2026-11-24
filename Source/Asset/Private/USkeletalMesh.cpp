/**
 * @file USkeletalMesh.cpp
 * @brief 본 데이터 검증과 공유 바인드 형상 및 GPU 스킨 가중치를 관리합니다.
 * @ingroup Asset
 */

#include "Core/Types.h"
#include "Asset/USkeletalMesh.h"
#include "Asset/USkeleton.h"
#include "Asset/FPhysicsAssetUtils.h"
#include "CoreUObject/Reflection/NewObject.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace Asset {

    /**
     * @brief 형상과 가중치를 검증하고 저장합니다.
     */
    bool Asset::USkeletalMesh::SetMeshData(Asset::FMeshData MeshData, Core::TArray<Asset::FMeshImportSkinWeight> SkinWeights) {
        if (SkinWeights.size() != MeshData.GetPositions().size() || SkinWeights.size() > 16000000) {
            return false;
        }

        Uint32 SkinMatrixCount{};

        for (const Asset::FMeshImportSkinWeight& SkinWeight : SkinWeights) {
            float Total{};

            for (Index_t Index{}; Index < Asset::FMeshImportSkinWeight::MaxInfluences; ++Index) {
                const Index_t BoneIndex{static_cast<Index_t>(SkinWeight.GetBoneIndices()[Index])};
                const float Weight{SkinWeight.GetWeights()[Index]};

                if (BoneIndex < 0 || BoneIndex >= Asset::MaxSkeletonBones || !std::isfinite(Weight) || Weight < 0.0f || Weight > 1.0f) {
                    return false;
                }

                SkinMatrixCount = std::max(SkinMatrixCount, static_cast<Uint32>(BoneIndex + 1));
                Total += Weight;
            }

            // 가중치 합은 1이어야 합니다. 합이 0인 정점은 본 영향이 없다는 뜻이며 바인드 위치에 그대로 남습니다.
            if (Total != 0.0f && std::abs(Total - 1.0f) > 0.0001f) {
                return false;
            }
        }

        if (!mGeometry.SetMeshData(std::move(MeshData))) {
            return false;
        }

        mSkinWeightBuffer.Reset();
        mSkinWeights = std::move(SkinWeights);
        mSkinMatrixCount = SkinMatrixCount;

        // 형상이 바뀌었으므로 정점으로 만든 물리 에셋을 버립니다.
        mGeneratedPhysicsAsset.reset();
        mGeneratedPhysicsAssetSkeleton = nullptr;

        // 섹션과 정점이 바뀌었으므로 그 섹션으로 만든 천 에셋과 연결도 버립니다.
        mMeshClothingAssets.clear();
        mSectionClothingData.clear();

        return true;
    }

    /**
     * @brief 바인드 포즈의 CPU 형상을 반환합니다.
     */
    const Asset::FMeshData& Asset::USkeletalMesh::GetMeshData() const {
        return mGeometry.GetMeshData();
    }

    /**
     * @brief 정점별 스킨 가중치를 반환합니다.
     */
    const Core::TArray<Asset::FMeshImportSkinWeight>& Asset::USkeletalMesh::GetSkinWeights() const {
        return mSkinWeights;
    }

    Uint32 Asset::USkeletalMesh::GetSkinMatrixCount() const {
        return mSkinMatrixCount;
    }

    /**
     * @brief 공통 메시 드로 경로가 사용할 바인드 형상을 반환합니다.
     */
    Asset::UStaticMesh* Asset::USkeletalMesh::GetGeometry() {
        return &mGeometry;
    }

    /**
     * @brief 장치에 바인드 형상과 불변 가중치 버퍼를 준비합니다.
     */
    bool Asset::USkeletalMesh::CreateResources(ID3D11Device* Device) {
        if (Device == nullptr || !IsValid() || !mGeometry.CreateResources(Device)) {
            return false;
        }

        // 가중치는 에셋을 교체하기 전까지 변하지 않으므로, 같은 장치에 이미 만든 버퍼가 있으면 재사용합니다.
        if (mSkinWeightBuffer.IsCreatedOn(Device)) {
            return true;
        }

        // 임포트 클래스의 메모리 배치와 무관하게 uint4, float4 순으로 입력을 만듭니다.
        static_assert(sizeof(FSkinWeightVertex) == sizeof(Uint32) * 4 + sizeof(float) * 4);

        Core::TArray<FSkinWeightVertex> Vertices{};
        Vertices.resize(mSkinWeights.size());

        for (Index_t Index{}; Index < mSkinWeights.size(); ++Index) {
            std::memcpy(Vertices[Index].mBoneIndices.data(), mSkinWeights[Index].GetBoneIndices().data(), sizeof(Uint32) * 4);
            std::memcpy(Vertices[Index].mWeights.data(), mSkinWeights[Index].GetWeights().data(), sizeof(float) * 4);
        }

        // 객체마다 달라지는 포즈는 본 팔레트(FScene::UploadSkinning)에 있고, 이 버퍼는 같은 에셋을 쓰는 객체가 공유하므로 불변으로 만듭니다.
        FSkinWeightBuffer Buffer{};
        Core::FString Error{};

        if (!Buffer.Create(Device, Vertices, Error)) {
            return false;
        }

        mSkinWeightBuffer = std::move(Buffer);

        return true;
    }

    /**
     * @brief 형상과 가중치의 GPU 자원을 해제합니다.
     */
    void Asset::USkeletalMesh::ReleaseResources() {
        mGeometry.ReleaseResources();
        mSkinWeightBuffer.Reset();
    }

    /**
     * @brief 본 인덱스와 가중치를 담은 정점 버퍼를 입력 슬롯에 연결합니다.
     */
    void Asset::USkeletalMesh::BindSkinWeights(ID3D11DeviceContext* Context, Index_t Slot) const {
        mSkinWeightBuffer.Bind(Context, Slot);
    }

    /**
     * @brief 입력 슬롯의 스킨 가중치 버퍼 연결을 해제합니다.
     */
    void Asset::USkeletalMesh::UnbindSkinWeights(ID3D11DeviceContext* Context, Index_t Slot) {
        FSkinWeightBuffer::Unbind(Context, Slot);
    }

    /**
     * @brief 바인드 형상의 로컬 경계를 반환합니다.
     */
    const FBox& Asset::USkeletalMesh::GetBounds() const {
        return mGeometry.GetBounds();
    }

    /**
     * @brief 메시 전체를 하나의 바디로 다룰 때 사용할 충돌 형상을 반환합니다.
     */
    const PhysicsRuntime::UBodySetup* Asset::USkeletalMesh::GetBodySetup() const {
        // 바인드 형상을 보관하는 정적 메시가 경계 상자 바디 셋업을 함께 만듭니다.
        return mGeometry.GetBodySetup();
    }

    /**
     * @brief 정점 수를 반환합니다.
     */
    Uint32 Asset::USkeletalMesh::GetVertexCount() const {
        return mGeometry.GetVertexCount();
    }

    /**
     * @brief 인덱스 수를 반환합니다.
     */
    Uint32 Asset::USkeletalMesh::GetIndexCount() const {
        return mGeometry.GetIndexCount();
    }

    /**
     * @brief 스키닝에 필요한 형상과 본 데이터가 있는지 검사합니다.
     */
    bool Asset::USkeletalMesh::IsValid() const {
        return mGeometry.IsValid() && mSkinWeights.size() == GetVertexCount();
    }

    /**
     * @brief 이 메시가 참조하는 물리 에셋 경로를 반환합니다.
     */
    const Core::FString& Asset::USkeletalMesh::GetPhysicsAssetPath() const {
        return mPhysicsAssetPath;
    }

    /**
     * @brief 이 메시가 참조할 물리 에셋 경로를 설정합니다.
     */
    void Asset::USkeletalMesh::SetPhysicsAssetPath(Core::FString Path) {
        mPhysicsAssetPath = std::move(Path);
    }

    /**
     * @brief 물리 에셋이 지정되지 않았을 때 쓸 물리 에셋을 뼈와 정점으로 만들어 반환합니다.
     */
    PhysicsRuntime::UPhysicsAsset* Asset::USkeletalMesh::GetGeneratedPhysicsAsset(const Asset::USkeleton& Skeleton) {
        if (mGeneratedPhysicsAsset != nullptr && mGeneratedPhysicsAssetSkeleton == &Skeleton) {
            return mGeneratedPhysicsAsset.get();
        }

        auto PhysicsAsset{CoreUObject::NewObject<PhysicsRuntime::UPhysicsAsset>()};

        if (!Asset::FPhysicsAssetUtils::CreateFromSkeletalMesh(*PhysicsAsset, *this, Skeleton, Asset::FPhysAssetCreateParams{})) {
            return nullptr;
        }

        mGeneratedPhysicsAsset = std::move(PhysicsAsset);
        mGeneratedPhysicsAssetSkeleton = &Skeleton;

        return mGeneratedPhysicsAsset.get();
    }

    /**
     * @brief 천 에셋을 메시에 추가합니다.
     */
    Index_t Asset::USkeletalMesh::AddClothingAsset(std::unique_ptr<PhysicsRuntime::UClothingAssetCommon> ClothingAsset) {
        if (ClothingAsset == nullptr) {
            return InvalidIndex;
        }

        mMeshClothingAssets.push_back(std::move(ClothingAsset));

        return mMeshClothingAssets.size() - 1;
    }

    /**
     * @brief 메시가 소유한 천 에셋 목록을 반환합니다.
     */
    const Core::TArray<std::unique_ptr<PhysicsRuntime::UClothingAssetCommon>>& Asset::USkeletalMesh::GetMeshClothingAssets() const {
        return mMeshClothingAssets;
    }

    /**
     * @brief 식별자로 천 에셋을 찾습니다.
     */
    PhysicsRuntime::UClothingAssetCommon* Asset::USkeletalMesh::GetClothingAsset(const Core::FGuid& AssetGuid) const {
        for (const std::unique_ptr<PhysicsRuntime::UClothingAssetCommon>& ClothingAsset : mMeshClothingAssets) {
            if (ClothingAsset->GetGuid() == AssetGuid) {
                return ClothingAsset.get();
            }
        }

        return nullptr;
    }

    /**
     * @brief 섹션의 천 연결 데이터를 반환합니다.
     */
    const Asset::FClothingSectionData* Asset::USkeletalMesh::GetSectionClothingData(Index_t SubMeshIndex) const {
        if (SubMeshIndex >= GetMeshData().GetSubMeshes().size()) {
            return nullptr;
        }

        // 아직 연결한 적 없는 섹션은 빈 연결 데이터로 답합니다.
        static const Asset::FClothingSectionData EmptyClothingData{};

        return SubMeshIndex < mSectionClothingData.size() ? &mSectionClothingData[SubMeshIndex] : &EmptyClothingData;
    }

    /**
     * @brief 섹션의 천 연결 데이터를 저장합니다.
     */
    bool Asset::USkeletalMesh::SetSectionClothingData(Index_t SubMeshIndex, Asset::FClothingSectionData ClothingData) {
        const Index_t NumSubMeshes{GetMeshData().GetSubMeshes().size()};

        if (SubMeshIndex >= NumSubMeshes) {
            return false;
        }

        if (mSectionClothingData.size() < NumSubMeshes) {
            mSectionClothingData.resize(NumSubMeshes);
        }

        mSectionClothingData[SubMeshIndex] = std::move(ClothingData);

        return true;
    }

}
