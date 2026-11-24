/**
 * @file UClothingAssetCommon.cpp
 * @brief 스켈레탈 메시의 섹션 하나를 천으로 시뮬레이션할 데이터를 담은 에셋을 구현합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/UClothingAssetCommon.h"
#include "PhysicsRuntime/ClothingMeshUtils.h"

#include <algorithm>

namespace PhysicsRuntime {

    /**
     * @brief 에셋 내부 상수입니다.
     */
    namespace {
        /**
         * @brief 최대 거리가 이 값보다 작으면 고정 입자로 봅니다.
         */
        constexpr float KinematicThreshold{1.e-8f};
    }

    /**
     * @brief 상세 단계가 없는 빈 에셋을 구성합니다.
     */
    UClothingAssetCommon::UClothingAssetCommon() = default;

    /**
     * @brief 상세 단계 데이터를 정리합니다.
     */
    UClothingAssetCommon::~UClothingAssetCommon() = default;

    /**
     * @brief 빈 상세 단계를 하나 추가합니다.
     */
    Index_t UClothingAssetCommon::AddNewLod() {
        mLodData.emplace_back();

        return mLodData.size() - 1;
    }

    /**
     * @brief 상세 단계 번호가 유효한지 검사합니다.
     */
    bool UClothingAssetCommon::IsValidLod(Index_t LodIndex) const {
        return LodIndex < mLodData.size();
    }

    /**
     * @brief 상세 단계 수를 반환합니다.
     */
    Index_t UClothingAssetCommon::GetNumLods() const {
        return mLodData.size();
    }

    /**
     * @brief 역질량, 고정 입자 수, 뼈 영향 수를 다시 구합니다.
     */
    void UClothingAssetCommon::InvalidateCachedData() {
        for (FClothLODDataCommon& CurrentLodData : mLodData) {
            FClothPhysicalMeshData& PhysMesh{CurrentLodData.mPhysicalMeshData};
            Core::TArray<float>& InvMasses{PhysMesh.mInverseMasses};
            const Index_t NumVerts{PhysMesh.mVertices.size()};

            // 입자마다 닿은 삼각형 넓이를 더해 질량으로 씁니다.
            InvMasses.assign(NumVerts, 0.0f);

            for (Index_t TriangleBaseIndex{}; TriangleBaseIndex + 2 < PhysMesh.mIndices.size(); TriangleBaseIndex += 3) {
                const Uint32 Index0{PhysMesh.mIndices[TriangleBaseIndex]};
                const Uint32 Index1{PhysMesh.mIndices[TriangleBaseIndex + 1]};
                const Uint32 Index2{PhysMesh.mIndices[TriangleBaseIndex + 2]};
                const FVector AB{PhysMesh.mVertices[Index1] - PhysMesh.mVertices[Index0]};
                const FVector AC{PhysMesh.mVertices[Index2] - PhysMesh.mVertices[Index0]};
                const float TriangleArea{AB.Cross(AC).Length()};

                InvMasses[Index0] += TriangleArea;
                InvMasses[Index1] += TriangleArea;
                InvMasses[Index2] += TriangleArea;
            }

            PhysMesh.mNumFixedVerts = 0;

            const FPointWeightMap* const MaxDistances{PhysMesh.FindWeightMap(EWeightMapTargetCommon::MaxDistance)};
            const bool HasMaxDistances{MaxDistances != nullptr && MaxDistances->Num() > 0};
            float MassSum{};

            for (Index_t VertIndex{}; VertIndex < NumVerts; ++VertIndex) {
                if (HasMaxDistances && (*MaxDistances)[VertIndex] < KinematicThreshold) {
                    InvMasses[VertIndex] = 0.0f;
                    ++PhysMesh.mNumFixedVerts;
                } else {
                    MassSum += InvMasses[VertIndex];
                }
            }

            // 움직이는 입자의 평균 질량이 1이 되도록 맞춘 뒤 역수를 취합니다.
            if (MassSum > 0.0f) {
                const float MassScale{static_cast<float>(NumVerts - PhysMesh.mNumFixedVerts) / MassSum};

                for (float& InvMass : InvMasses) {
                    if (InvMass != 0.0f) {
                        InvMass = 1.0f / (InvMass * MassScale);
                    }
                }
            }

            // 가중치가 큰 순서로 채웠으므로 처음 나오는 0 앞까지가 영향 수입니다.
            for (FClothVertBoneData& BoneData : PhysMesh.mBoneData) {
                BoneData.mNumInfluences = MaxTotalInfluences;

                for (Index_t InfluenceIndex{}; InfluenceIndex < MaxTotalInfluences; ++InfluenceIndex) {
                    if (BoneData.mBoneWeights[InfluenceIndex] == 0.0f) {
                        BoneData.mNumInfluences = InfluenceIndex;

                        break;
                    }
                }
            }
        }
    }

    /**
     * @brief 사용 뼈 이름마다 스켈레톤의 뼈 번호를 다시 찾습니다.
     */
    void UClothingAssetCommon::RefreshBoneMapping(const Core::TArray<Core::FString>& SkeletonBoneNames) {
        mUsedBoneIndices.assign(mUsedBoneNames.size(), InvalidIndex);

        for (Index_t BoneNameIndex{}; BoneNameIndex < mUsedBoneNames.size(); ++BoneNameIndex) {
            const auto Found{std::find(SkeletonBoneNames.begin(), SkeletonBoneNames.end(), mUsedBoneNames[BoneNameIndex])};

            if (Found != SkeletonBoneNames.end()) {
                mUsedBoneIndices[BoneNameIndex] = static_cast<Index_t>(Found - SkeletonBoneNames.begin());
            }
        }
    }

    /**
     * @brief 시뮬레이션 좌표의 기준 뼈를 루트로 정합니다.
     */
    void UClothingAssetCommon::CalculateReferenceBoneIndex() {
        mReferenceBoneIndex = 0;
    }

    /**
     * @brief 이웃한 상세 단계 사이에서 입자 위치를 옮길 데이터를 만듭니다.
     */
    void UClothingAssetCommon::BuildLodTransitionData() {
        const Index_t NumLods{GetNumLods()};

        for (Index_t LodIndex{}; LodIndex < NumLods; ++LodIndex) {
            FClothLODDataCommon& CurrentLod{mLodData[LodIndex]};
            const FClothPhysicalMeshData& CurrentPhysMesh{CurrentLod.mPhysicalMeshData};
            const ClothingMeshUtils::ClothMeshDesc CurrentMeshDesc{CurrentPhysMesh.mVertices, CurrentPhysMesh.mNormals, CurrentPhysMesh.mIndices};

            CurrentLod.mTransitionUpSkinData.clear();
            CurrentLod.mTransitionDownSkinData.clear();

            if (LodIndex > 0) {
                const FClothPhysicalMeshData& PrevPhysMesh{mLodData[LodIndex - 1].mPhysicalMeshData};
                const ClothingMeshUtils::ClothMeshDesc PrevMeshDesc{PrevPhysMesh.mVertices, PrevPhysMesh.mNormals, PrevPhysMesh.mIndices};

                ClothingMeshUtils::GenerateMeshToMeshSkinningData(CurrentLod.mTransitionUpSkinData, CurrentMeshDesc, nullptr, PrevMeshDesc);
            }

            if (LodIndex + 1 < NumLods) {
                const FClothPhysicalMeshData& NextPhysMesh{mLodData[LodIndex + 1].mPhysicalMeshData};
                const ClothingMeshUtils::ClothMeshDesc NextMeshDesc{NextPhysMesh.mVertices, NextPhysMesh.mNormals, NextPhysMesh.mIndices};

                ClothingMeshUtils::GenerateMeshToMeshSkinningData(CurrentLod.mTransitionDownSkinData, CurrentMeshDesc, nullptr, NextMeshDesc);
            }
        }
    }
}
