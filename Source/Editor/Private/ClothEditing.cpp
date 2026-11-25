/**
 * @file ClothEditing.cpp
 * @brief 에디터 명령과 메시 뷰어가 함께 쓰는 천 에셋 만들기, 빼기, 설정 목록을 구현합니다.
 * @ingroup Editor
 */

#include "ClothEditing.h"
#include "Asset/FClothingAssetFactory.h"
#include "Asset/USkeletalMesh.h"
#include "Asset/USkeleton.h"
#include "PhysicsRuntime/UClothConfigNv.h"
#include "PhysicsRuntime/UClothingAssetCommon.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Editor::ClothEditing {

    /**
     * @brief 스켈레톤의 뼈 이름을 뼈 번호 순서로 모읍니다.
     */
    Core::TArray<Core::FString> GetSkeletonBoneNames(const Asset::USkeleton& Skeleton) {
        Core::TArray<Core::FString> BoneNames{};
        BoneNames.reserve(Skeleton.GetBones().size());

        for (const Asset::FMeshImportBone& Bone : Skeleton.GetBones()) {
            BoneNames.push_back(Bone.GetName());
        }

        return BoneNames;
    }

    /**
     * @brief 섹션 하나로 천 에셋을 만들어 메시에 넣고 섹션에 연결합니다.
     */
    PhysicsRuntime::UClothingAssetCommon* CreateClothFromSection(Asset::USkeletalMesh& Mesh, const Asset::USkeleton& Skeleton, Index_t SubMeshIndex, PhysicsRuntime::UPhysicsAsset* PhysicsAsset, const Core::FString& PhysicsAssetPath, Core::FString& Error) {
        std::unique_ptr<PhysicsRuntime::UClothingAssetCommon> NewAsset{Asset::FClothingAssetFactory::CreateFromSkeletalMesh(Mesh, Skeleton, SubMeshIndex, PhysicsAsset)};

        if (NewAsset == nullptr) {
            Error = "Cannot create a clothing asset from section " + std::to_string(SubMeshIndex) + ". See the Console for details.";

            return nullptr;
        }

        NewAsset->mPhysicsAssetPath = PhysicsAssetPath;

        // 최대 거리를 칠하기 전에도 움직이도록 위쪽 일부 입자만 고정하고 나머지는 움직일 수 있게 둡니다.
        constexpr float FixedBandRatio{0.2f};
        constexpr float MovableMaxDistance{0.5f};
        PhysicsRuntime::FClothLODDataCommon& LodData{NewAsset->mLodData[0]};
        PhysicsRuntime::FClothPhysicalMeshData& PhysMesh{LodData.mPhysicalMeshData};
        float MinZ{std::numeric_limits<float>::max()};
        float MaxZ{std::numeric_limits<float>::lowest()};

        for (const FVector& Vertex : PhysMesh.mVertices) {
            MinZ = std::min(MinZ, Vertex.z);
            MaxZ = std::max(MaxZ, Vertex.z);
        }

        const float FixedAboveZ{MaxZ - (MaxZ - MinZ) * FixedBandRatio};
        PhysicsRuntime::FPointWeightMap& MaxDistances{PhysMesh.AddWeightMap(PhysicsRuntime::EWeightMapTargetCommon::MaxDistance)};

        for (Index_t VertIndex{}; VertIndex < PhysMesh.mVertices.size(); ++VertIndex) {
            MaxDistances[VertIndex] = PhysMesh.mVertices[VertIndex].z >= FixedAboveZ ? 0.0f : MovableMaxDistance;
        }

        for (PhysicsRuntime::FPointWeightMap& PointWeightMap : LodData.mPointWeightMaps) {
            if (PointWeightMap.mCurrentTarget == PhysicsRuntime::EWeightMapTargetCommon::MaxDistance) {
                PointWeightMap.mValues = MaxDistances.mValues;
            }
        }

        // 고정 입자가 바뀌었으므로 역질량과 천끼리 부딪힐 입자를 다시 구합니다.
        NewAsset->InvalidateCachedData();
        NewAsset->BuildSelfCollisionData();

        PhysicsRuntime::UClothingAssetCommon& ClothingAsset{*NewAsset};
        Mesh.AddClothingAsset(std::move(NewAsset));

        if (!Asset::FClothingAssetFactory::BindToSkeletalMesh(Mesh, ClothingAsset, SubMeshIndex, 0)) {
            Mesh.RemoveClothingAsset(ClothingAsset.GetGuid());
            Error = "Cannot bind the clothing asset to section " + std::to_string(SubMeshIndex) + ". See the Console for details.";

            return nullptr;
        }

        return &ClothingAsset;
    }

    /**
     * @brief 설정의 실수 항목 이름과 값 위치를 모읍니다.
     */
    Core::TArray<std::pair<Core::FString, float*>> GetConfigFloats(PhysicsRuntime::UClothConfigNv& Config) {
        return Core::TArray<std::pair<Core::FString, float*>>{
            {"vertical.stiffness", &Config.mVerticalConstraint.mStiffness},
            {"vertical.stiffnessmultiplier", &Config.mVerticalConstraint.mStiffnessMultiplier},
            {"vertical.stretchlimit", &Config.mVerticalConstraint.mStretchLimit},
            {"vertical.compressionlimit", &Config.mVerticalConstraint.mCompressionLimit},
            {"horizontal.stiffness", &Config.mHorizontalConstraint.mStiffness},
            {"horizontal.stiffnessmultiplier", &Config.mHorizontalConstraint.mStiffnessMultiplier},
            {"horizontal.stretchlimit", &Config.mHorizontalConstraint.mStretchLimit},
            {"horizontal.compressionlimit", &Config.mHorizontalConstraint.mCompressionLimit},
            {"bend.stiffness", &Config.mBendConstraint.mStiffness},
            {"bend.stiffnessmultiplier", &Config.mBendConstraint.mStiffnessMultiplier},
            {"bend.stretchlimit", &Config.mBendConstraint.mStretchLimit},
            {"bend.compressionlimit", &Config.mBendConstraint.mCompressionLimit},
            {"shear.stiffness", &Config.mShearConstraint.mStiffness},
            {"shear.stiffnessmultiplier", &Config.mShearConstraint.mStiffnessMultiplier},
            {"shear.stretchlimit", &Config.mShearConstraint.mStretchLimit},
            {"shear.compressionlimit", &Config.mShearConstraint.mCompressionLimit},
            {"selfcollisionradius", &Config.mSelfCollisionRadius},
            {"selfcollisionstiffness", &Config.mSelfCollisionStiffness},
            {"selfcollisioncullscale", &Config.mSelfCollisionCullScale},
            {"friction", &Config.mFriction},
            {"winddrag", &Config.mWindDragCoefficient},
            {"windlift", &Config.mWindLiftCoefficient},
            {"solverfrequency", &Config.mSolverFrequency},
            {"stiffnessfrequency", &Config.mStiffnessFrequency},
            {"gravityscale", &Config.mGravityScale},
            {"tetherstiffness", &Config.mTetherStiffness},
            {"tetherlimit", &Config.mTetherLimit},
            {"collisionthickness", &Config.mCollisionThickness},
            {"animdrivespring", &Config.mAnimDriveSpringStiffness},
            {"animdrivedamper", &Config.mAnimDriveDamperStiffness},
        };
    }

    /**
     * @brief 칠하기 대상 번호의 표시 이름을 반환합니다.
     */
    const char* GetPaintTargetName(int TargetIndex) {
        switch (TargetIndex) {
            case 1:
                return "Backstop Distance";
            case 2:
                return "Backstop Radius";
            case 3:
                return "Anim Drive Multiplier";
            default:
                return "Max Distance";
        }
    }

    /**
     * @brief 칠하기 대상 번호를 가중치 맵 대상으로 바꿉니다.
     */
    PhysicsRuntime::EWeightMapTargetCommon GetPaintTarget(int TargetIndex) {
        switch (TargetIndex) {
            case 1:
                return PhysicsRuntime::EWeightMapTargetCommon::BackstopDistance;
            case 2:
                return PhysicsRuntime::EWeightMapTargetCommon::BackstopRadius;
            case 3:
                return PhysicsRuntime::EWeightMapTargetCommon::AnimDriveMultiplier;
            default:
                return PhysicsRuntime::EWeightMapTargetCommon::MaxDistance;
        }
    }

    /**
     * @brief 칠한 가중치 맵을 천 에셋에 반영합니다.
     */
    void ApplyPaintedWeights(Asset::USkeletalMesh& Mesh, PhysicsRuntime::UClothingAssetCommon& ClothingAsset) {
        // 시뮬레이션 메시의 맵을 편집용 가중치 맵 목록에도 그대로 남깁니다.
        for (PhysicsRuntime::FClothLODDataCommon& Lod : ClothingAsset.mLodData) {
            for (const auto& [TargetValue, Map] : Lod.mPhysicalMeshData.mWeightMaps) {
                const auto Target{static_cast<PhysicsRuntime::EWeightMapTargetCommon>(TargetValue)};
                auto Found{std::find_if(Lod.mPointWeightMaps.begin(), Lod.mPointWeightMaps.end(), [Target](const PhysicsRuntime::FPointWeightMap& PointWeightMap) {
                    return PointWeightMap.mCurrentTarget == Target;
                })};

                if (Found == Lod.mPointWeightMaps.end()) {
                    PhysicsRuntime::FPointWeightMap& NewMap{Lod.mPointWeightMaps.emplace_back()};
                    NewMap.mName = GetPaintTargetName(Target == PhysicsRuntime::EWeightMapTargetCommon::BackstopDistance ? 1 : Target == PhysicsRuntime::EWeightMapTargetCommon::BackstopRadius ? 2 : Target == PhysicsRuntime::EWeightMapTargetCommon::AnimDriveMultiplier ? 3 : 0);
                    NewMap.mCurrentTarget = Target;
                    NewMap.mEnabled = true;
                    NewMap.mValues = Map.mValues;
                } else {
                    Found->mValues = Map.mValues;
                }
            }
        }

        // 최대 거리가 바뀌면 고정 입자와 역질량, 천끼리 부딪힐 입자가 달라집니다.
        ClothingAsset.InvalidateCachedData();
        ClothingAsset.BuildSelfCollisionData();

        // 세 입자가 모두 고정인 삼각형에 붙은 정점은 스키닝만 쓰도록 섹션 정점 표시를 다시 정합니다.
        const Index_t NumSubMeshes{Mesh.GetMeshData().GetSubMeshes().size()};

        for (Index_t SubMeshIndex{}; SubMeshIndex < NumSubMeshes; ++SubMeshIndex) {
            const Asset::FClothingSectionData* SectionData{Mesh.GetSectionClothingData(SubMeshIndex)};

            if (SectionData == nullptr || !SectionData->IsValid() || !(SectionData->mAssetGuid == ClothingAsset.GetGuid()) || !ClothingAsset.IsValidLod(SectionData->mAssetLodIndex)) {
                continue;
            }

            const PhysicsRuntime::FPointWeightMap* const MaxDistances{ClothingAsset.mLodData[SectionData->mAssetLodIndex].mPhysicalMeshData.FindWeightMap(PhysicsRuntime::EWeightMapTargetCommon::MaxDistance)};
            Asset::FClothingSectionData Updated{*SectionData};

            for (PhysicsRuntime::FMeshToMeshVertData& VertData : Updated.mClothMappingData) {
                const bool AllFixed{MaxDistances != nullptr && MaxDistances->Num() > 0 && MaxDistances->AreAllBelowThreshold(VertData.mSourceMeshVertIndices[0], VertData.mSourceMeshVertIndices[1], VertData.mSourceMeshVertIndices[2])};

                VertData.mSourceMeshVertIndices[3] = AllFixed ? PhysicsRuntime::FMeshToMeshVertData::NoSimulationFlag : 0u;
            }

            // 연결 데이터를 다시 넣으면 렌더러가 쓰는 천 정점 버퍼도 다음에 새로 만듭니다.
            Mesh.SetSectionClothingData(SubMeshIndex, std::move(Updated));
        }
    }

    /**
     * @brief 설정의 벡터 항목 이름과 값 위치를 모읍니다.
     */
    Core::TArray<std::pair<Core::FString, FVector*>> GetConfigVectors(PhysicsRuntime::UClothConfigNv& Config) {
        return Core::TArray<std::pair<Core::FString, FVector*>>{
            {"damping", &Config.mDamping},
            {"lineardrag", &Config.mLinearDrag},
            {"angulardrag", &Config.mAngularDrag},
            {"linearinertia", &Config.mLinearInertiaScale},
            {"angularinertia", &Config.mAngularInertiaScale},
            {"centrifugalinertia", &Config.mCentrifugalInertiaScale},
            {"gravityoverride", &Config.mGravityOverride},
        };
    }
}
