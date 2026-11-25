/**
 * @file ClothEditing.cpp
 * @brief 에디터 명령과 메시 뷰어가 함께 쓰는 천 에셋 만들기, 설정 항목 표, 가중치 칠하기 도구를 구현합니다.
 * @ingroup Editor
 */

#include "ClothEditing.h"
#include "Asset/FClothingAssetFactory.h"
#include "Asset/USkeletalMesh.h"
#include "Asset/USkeleton.h"
#include "PhysicsRuntime/FClothPhysicalMeshData.h"
#include "PhysicsRuntime/UClothConfigNv.h"
#include "PhysicsRuntime/UClothingAssetCommon.h"

#include <algorithm>
#include <cmath>
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

    namespace {
        /**
         * @brief 강성 항목 하나를 만듭니다.
         * @param[in] Key 콘솔 이름입니다.
         * @param[in] Label 패널 이름입니다.
         * @param[in] Format 표시 형식입니다.
         * @param[in] Min 최솟값입니다.
         * @param[in] Max 최댓값입니다.
         * @param[in] Tooltip 설명입니다.
         * @param[in] GetFloat 값 위치를 구하는 함수입니다.
         * @return 강성 묶음의 설정 항목입니다.
         */
        FClothConfigField MakeStiffnessField(const char* Key, const char* Label, const char* Format, float Min, float Max, const char* Tooltip, float* (*GetFloat)(PhysicsRuntime::UClothConfigNv&)) {
            return FClothConfigField{.mKey = Key, .mLabel = Label, .mGroup = StiffnessGroup, .mFormat = Format, .mDisplayScale = 1.0f, .mMin = Min, .mMax = Max, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 0.0f, 0.0f}, .mTooltip = Tooltip, .mGetFloat = GetFloat};
        }
    }

    /**
     * @brief 모든 천 설정 항목을 패널 표시 순서로 반환합니다.
     */
    const Core::TArray<FClothConfigField>& GetConfigFields() {
        using PhysicsRuntime::UClothConfigNv;

        static const Core::TArray<FClothConfigField> Fields{
            // 강성 표는 세로, 가로, 굽힘, 전단 순서로 한 행에 세기, 배율, 늘어남 한계, 줄어듦 한계 네 칸입니다.
            MakeStiffnessField("vertical.stiffness", "Vertical Stiffness", "%.2f", 0.0f, 1.0f, "세로 연결의 세기입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mVerticalConstraint.mStiffness; }),
            MakeStiffnessField("vertical.stiffnessmultiplier", "Vertical Multiplier", "%.2f", 0.0f, 1.0f, "세로 연결의 느슨한 구간 배율입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mVerticalConstraint.mStiffnessMultiplier; }),
            MakeStiffnessField("vertical.stretchlimit", "Vertical Stretch Limit", "%.2fx", 1.0f, 10.0f, "세로 연결의 늘어남 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mVerticalConstraint.mStretchLimit; }),
            MakeStiffnessField("vertical.compressionlimit", "Vertical Compress Limit", "%.2fx", 0.01f, 1.0f, "세로 연결의 줄어듦 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mVerticalConstraint.mCompressionLimit; }),
            MakeStiffnessField("horizontal.stiffness", "Horizontal Stiffness", "%.2f", 0.0f, 1.0f, "가로 연결의 세기입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mHorizontalConstraint.mStiffness; }),
            MakeStiffnessField("horizontal.stiffnessmultiplier", "Horizontal Multiplier", "%.2f", 0.0f, 1.0f, "가로 연결의 느슨한 구간 배율입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mHorizontalConstraint.mStiffnessMultiplier; }),
            MakeStiffnessField("horizontal.stretchlimit", "Horizontal Stretch Limit", "%.2fx", 1.0f, 10.0f, "가로 연결의 늘어남 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mHorizontalConstraint.mStretchLimit; }),
            MakeStiffnessField("horizontal.compressionlimit", "Horizontal Compress Limit", "%.2fx", 0.01f, 1.0f, "가로 연결의 줄어듦 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mHorizontalConstraint.mCompressionLimit; }),
            MakeStiffnessField("bend.stiffness", "Bend Stiffness", "%.2f", 0.0f, 1.0f, "굽힘 연결의 세기입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mBendConstraint.mStiffness; }),
            MakeStiffnessField("bend.stiffnessmultiplier", "Bend Multiplier", "%.2f", 0.0f, 1.0f, "굽힘 연결의 느슨한 구간 배율입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mBendConstraint.mStiffnessMultiplier; }),
            MakeStiffnessField("bend.stretchlimit", "Bend Stretch Limit", "%.2fx", 1.0f, 10.0f, "굽힘 연결의 늘어남 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mBendConstraint.mStretchLimit; }),
            MakeStiffnessField("bend.compressionlimit", "Bend Compress Limit", "%.2fx", 0.01f, 1.0f, "굽힘 연결의 줄어듦 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mBendConstraint.mCompressionLimit; }),
            MakeStiffnessField("shear.stiffness", "Shear Stiffness", "%.2f", 0.0f, 1.0f, "전단 연결의 세기입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mShearConstraint.mStiffness; }),
            MakeStiffnessField("shear.stiffnessmultiplier", "Shear Multiplier", "%.2f", 0.0f, 1.0f, "전단 연결의 느슨한 구간 배율입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mShearConstraint.mStiffnessMultiplier; }),
            MakeStiffnessField("shear.stretchlimit", "Shear Stretch Limit", "%.2fx", 1.0f, 10.0f, "전단 연결의 늘어남 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mShearConstraint.mStretchLimit; }),
            MakeStiffnessField("shear.compressionlimit", "Shear Compress Limit", "%.2fx", 0.01f, 1.0f, "전단 연결의 줄어듦 한계입니다. 기본값 1.", [](UClothConfigNv& C) { return &C.mShearConstraint.mCompressionLimit; }),

            FClothConfigField{.mKey = "solverfrequency", .mLabel = "Solver Frequency", .mGroup = "Simulation", .mFormat = "%.0f Hz", .mMin = 30.0f, .mMax = 1000.0f, .mSpeed = 1.0f, .mDefault = FVector{120.0f, 0.0f, 0.0f},
                .mTooltip = "1초에 천의 연결을 몇 번 풀지입니다.\n높이면 천이 덜 늘어나고 안정되지만 계산이 늘어납니다.\n낮추면 잘 늘어나고 통통 튑니다.\n기본값 120 Hz.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mSolverFrequency; }},
            FClothConfigField{.mKey = "stiffnessfrequency", .mLabel = "Stiffness Frequency", .mGroup = "Simulation", .mFormat = "%.0f Hz", .mMin = 1.0f, .mMax = 1000.0f, .mSpeed = 1.0f, .mDefault = FVector{100.0f, 0.0f, 0.0f},
                .mTooltip = "세기, 감쇠, 끌림 값을 '1초에 이 횟수만큼 적용한 결과'로 보는 기준입니다.\n낮추면 같은 세기 값이라도 연결이 약해지고, 높이면 강해집니다.\n기본값 100 Hz.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mStiffnessFrequency; }},
            FClothConfigField{.mKey = "gravityscale", .mLabel = "Gravity Scale", .mGroup = "Simulation", .mFormat = "%.2fx", .mMin = 0.0f, .mMax = 10.0f, .mSpeed = 0.01f, .mDefault = FVector{1.0f, 0.0f, 0.0f},
                .mTooltip = "월드 중력에 곱하는 배율입니다.\n0이면 무중력처럼 떠 있고, 2면 두 배로 무겁게 처집니다.\nOverride Gravity를 켜면 쓰지 않습니다.\n기본값 1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mGravityScale; }},
            FClothConfigField{.mKey = "gravityoverride", .mLabel = "Gravity", .mGroup = "Simulation", .mFormat = "%.2f m/s2", .mMin = -50.0f, .mMax = 50.0f, .mSpeed = 0.05f, .mDefault = FVector{0.0f, 0.0f, 0.0f},
                .mTooltip = "월드 중력 대신 쓸 중력 가속도입니다(m/s2, x y z).\n예를 들어 (0, 0, -9.81)이면 보통 중력입니다.\nOverride Gravity를 켰을 때만 씁니다.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mGravityOverride; }},

            FClothConfigField{.mKey = "damping", .mLabel = "Damping", .mGroup = "Damping & Drag", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 0.99f, .mSpeed = 0.005f, .mDefault = FVector{0.4f, 0.4f, 0.4f},
                .mTooltip = "입자 속도를 매번 줄이는 비율입니다(x y z 축별).\n높이면 천이 꿀 속처럼 천천히 움직이고 흔들림이 빨리 멈춥니다.\n0이면 줄이지 않습니다. 1보다 작아야 합니다.\n기본값 0.4.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mDamping; }},
            FClothConfigField{.mKey = "lineardrag", .mLabel = "Linear Drag", .mGroup = "Damping & Drag", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 0.99f, .mSpeed = 0.005f, .mDefault = FVector{0.2f, 0.2f, 0.2f},
                .mTooltip = "몸이 움직일 때 천이 몸을 따라 끌려가는 정도입니다(축별).\n높이면 걷거나 뛸 때 천이 몸에 더 바짝 따라옵니다.\n기본값 0.2.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mLinearDrag; }},
            FClothConfigField{.mKey = "angulardrag", .mLabel = "Angular Drag", .mGroup = "Damping & Drag", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 0.99f, .mSpeed = 0.005f, .mDefault = FVector{0.2f, 0.2f, 0.2f},
                .mTooltip = "몸이 돌 때 천이 같이 돌려지는 정도입니다(축별).\n높이면 회전할 때 천이 덜 뒤처집니다.\n기본값 0.2.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mAngularDrag; }},

            FClothConfigField{.mKey = "linearinertia", .mLabel = "Linear Inertia", .mGroup = "Inertia", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 1.0f, 1.0f},
                .mTooltip = "몸이 갑자기 움직이거나 멈출 때 그 가속을 천에 얼마나 전할지입니다(축별).\n1이 물리적으로 맞고, 0이면 몸이 갑자기 움직여도 천이 휘날리지 않습니다.\n기본값 1.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mLinearInertiaScale; }},
            FClothConfigField{.mKey = "angularinertia", .mLabel = "Angular Inertia", .mGroup = "Inertia", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 1.0f, 1.0f},
                .mTooltip = "몸이 갑자기 돌기 시작하거나 멈출 때 그 회전 가속을 천에 얼마나 전할지입니다(축별).\n기본값 1.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mAngularInertiaScale; }},
            FClothConfigField{.mKey = "centrifugalinertia", .mLabel = "Centrifugal Inertia", .mGroup = "Inertia", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 1.0f, 1.0f},
                .mTooltip = "몸이 돌 때 바깥으로 쏠리는 힘(원심력)을 천에 얼마나 전할지입니다(축별).\n0이면 빙글 돌아도 치마가 바깥으로 퍼지지 않습니다.\n기본값 1.",
                .mGetVector = [](UClothConfigNv& C) { return &C.mCentrifugalInertiaScale; }},

            FClothConfigField{.mKey = "tetherstiffness", .mLabel = "Tether Stiffness", .mGroup = "Tether", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 0.0f, 0.0f},
                .mTooltip = "고정된 입자(최대 거리 0)와 각 입자를 잇는 보이지 않는 끈의 세기입니다.\n1이면 끈 길이 이상 멀어지지 못하고, 낮추면 고무줄처럼 늘어납니다.\n기본값 1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mTetherStiffness; }},
            FClothConfigField{.mKey = "tetherlimit", .mLabel = "Tether Limit", .mGroup = "Tether", .mFormat = "%.2fx", .mMin = 0.0f, .mMax = 4.0f, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 0.0f, 0.0f},
                .mTooltip = "끈 길이에 곱하는 배율입니다.\n1이면 처음 모양에서의 거리, 1.2면 그보다 20% 더 멀어질 수 있습니다.\n0이면 끈을 쓰지 않습니다.\n기본값 1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mTetherLimit; }},

            FClothConfigField{.mKey = "collisionthickness", .mLabel = "Thickness", .mGroup = "Collision", .mFormat = "%.2f cm", .mDisplayScale = 100.0f, .mMin = 0.0f, .mMax = 0.1f, .mSpeed = 0.05f, .mDefault = FVector{0.01f, 0.0f, 0.0f},
                .mTooltip = "몸 충돌체 표면에서 천을 이만큼 띄웁니다.\n높이면 천이 몸을 덜 뚫지만 몸에서 떠 보입니다.\n기본값 1 cm.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mCollisionThickness; }},
            FClothConfigField{.mKey = "friction", .mLabel = "Friction", .mGroup = "Collision", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.005f, .mDefault = FVector{0.1f, 0.0f, 0.0f},
                .mTooltip = "천이 몸 충돌체에 닿았을 때 미끄러지지 않게 잡는 정도입니다.\n높이면 천이 몸에 붙어 덜 미끄러집니다.\n기본값 0.1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mFriction; }},

            FClothConfigField{.mKey = "selfcollisionradius", .mLabel = "Radius", .mGroup = "Self Collision", .mFormat = "%.2f cm", .mDisplayScale = 100.0f, .mMin = 0.0f, .mMax = 0.1f, .mSpeed = 0.05f, .mDefault = FVector{0.0f, 0.0f, 0.0f},
                .mTooltip = "천끼리 부딪힐 때 입자마다 둘 구의 반지름입니다.\n0이면 천끼리 부딪히지 않습니다.\n켜면 천이 자기 자신을 덜 뚫지만 계산이 크게 늘어납니다.\n기본값 0.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mSelfCollisionRadius; }},
            FClothConfigField{.mKey = "selfcollisionstiffness", .mLabel = "Stiffness", .mGroup = "Self Collision", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.005f, .mDefault = FVector{0.0f, 0.0f, 0.0f},
                .mTooltip = "천끼리 겹쳤을 때 서로 밀어내는 세기입니다.\n0이면 천끼리 부딪히지 않습니다.\n기본값 0.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mSelfCollisionStiffness; }},
            FClothConfigField{.mKey = "selfcollisioncullscale", .mLabel = "Cull Scale", .mGroup = "Self Collision", .mFormat = "%.2fx", .mMin = 0.0f, .mMax = 4.0f, .mSpeed = 0.005f, .mDefault = FVector{1.0f, 0.0f, 0.0f},
                .mTooltip = "반지름 x 이 값 안에 이미 고른 입자가 있으면 그 입자는 천끼리 충돌 계산에서 뺍니다.\n높이면 계산할 입자가 줄어 빨라지지만 덜 정확해집니다.\n낮추면 정확해지지만 느려집니다.\n기본값 1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mSelfCollisionCullScale; }},

            FClothConfigField{.mKey = "winddrag", .mLabel = "Drag", .mGroup = "Wind", .mFormat = "%.4f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.0001f, .mDefault = FVector{0.0002f, 0.0f, 0.0f},
                .mTooltip = "바람이 천 면을 바람 방향으로 미는 정도입니다.\n높이면 바람에 옆으로 더 밀립니다.\nWind Method가 Accurate일 때만 씁니다.\n기본값 0.0002.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mWindDragCoefficient; }},
            FClothConfigField{.mKey = "windlift", .mLabel = "Lift", .mGroup = "Wind", .mFormat = "%.4f", .mMin = 0.0f, .mMax = 1.0f, .mSpeed = 0.0001f, .mDefault = FVector{0.0002f, 0.0f, 0.0f},
                .mTooltip = "바람이 천 면을 바람과 수직 방향으로 띄우는 정도입니다.\n높이면 바람에 천이 더 잘 떠오릅니다.\nWind Method가 Accurate일 때만 씁니다.\n기본값 0.0002.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mWindLiftCoefficient; }},

            FClothConfigField{.mKey = "animdrivespring", .mLabel = "Spring", .mGroup = "Anim Drive", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 10.0f, .mSpeed = 0.01f, .mDefault = FVector{1.0f, 0.0f, 0.0f},
                .mTooltip = "Anim Drive Multiplier를 칠한 입자를 애니메이션 위치로 당기는 세기입니다.\n높이면 천이 애니메이션 모양을 더 잘 유지합니다.\n칠한 맵이 없으면 쓰지 않습니다.\n기본값 1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mAnimDriveSpringStiffness; }},
            FClothConfigField{.mKey = "animdrivedamper", .mLabel = "Damper", .mGroup = "Anim Drive", .mFormat = "%.2f", .mMin = 0.0f, .mMax = 10.0f, .mSpeed = 0.01f, .mDefault = FVector{1.0f, 0.0f, 0.0f},
                .mTooltip = "애니메이션 위치로 당길 때 입자와 애니메이션의 속도 차이를 줄이는 세기입니다.\n높이면 당겨질 때 덜 출렁입니다.\n기본값 1.",
                .mGetFloat = [](UClothConfigNv& C) { return &C.mAnimDriveDamperStiffness; }},
        };

        return Fields;
    }

    /**
     * @brief 강성 표의 행 이름과 설명을 반환합니다.
     */
    const Core::TArray<FClothLabel>& GetStiffnessRows() {
        static const Core::TArray<FClothLabel> Rows{
            {"Vertical", "중력 방향(아래로 늘어지는 방향)으로 이웃한 입자 사이의 연결입니다.\n낮추면 천이 아래로 잘 늘어납니다."},
            {"Horizontal", "중력과 수직인 방향으로 이웃한 입자 사이의 연결입니다.\n낮추면 천이 옆으로 잘 늘어납니다."},
            {"Bend", "한 칸 건너 이웃한 입자 사이의 연결이며 천이 접히는 정도를 정합니다.\n낮추면 잘 구겨지고, 높이면 빳빳해집니다."},
            {"Shear", "대각선으로 이웃한 입자 사이의 연결이며 비틀림을 막습니다.\n낮추면 네모 칸이 마름모처럼 잘 찌그러집니다."},
        };

        return Rows;
    }

    /**
     * @brief 강성 표의 열 이름과 설명을 반환합니다.
     */
    const Core::TArray<FClothLabel>& GetStiffnessColumns() {
        static const Core::TArray<FClothLabel> Columns{
            {"Stiffness", "한 번 풀 때 늘어나거나 줄어든 연결을 원래 길이로 얼마나 되돌릴지입니다.\n1이면 바로 되돌리고, 0이면 되돌리지 않습니다.\n0~1, 기본값 1."},
            {"Multiplier", "연결 길이가 아래 두 한계 사이(느슨한 구간)에 있을 때 세기에 곱하는 값입니다.\n1이면 느슨한 구간 없이 똑같이 당기고, 낮추면 그 구간에서는 고무처럼 느슨해집니다.\n0~1, 기본값 1."},
            {"Stretch", "원래 길이의 몇 배까지 늘어난 것을 느슨한 구간으로 볼지입니다.\n예: 1.1이면 10%까지는 Multiplier만큼 약하게 당깁니다.\n1이면 느슨한 구간이 없습니다. 1 이상, 기본값 1."},
            {"Compress", "원래 길이의 몇 배까지 줄어든 것을 느슨한 구간으로 볼지입니다.\n예: 0.9이면 10%까지 줄어드는 것은 Multiplier만큼 약하게 밀어냅니다.\n1이면 느슨한 구간이 없습니다. 0~1, 기본값 1."},
        };

        return Columns;
    }

    /**
     * @brief 강성 표가 아닌 설정 묶음의 이름과 설명을 반환합니다.
     */
    const Core::TArray<FClothLabel>& GetConfigGroups() {
        static const Core::TArray<FClothLabel> Groups{
            {"Simulation", "얼마나 자주 풀지와 중력을 정합니다."},
            {"Damping & Drag", "천의 움직임이 얼마나 빨리 잦아들고 몸을 얼마나 따라오는지 정합니다."},
            {"Inertia", "몸이 가속하거나 돌 때 그 힘을 천에 얼마나 전할지 정합니다."},
            {"Tether", "고정된 입자에서 너무 멀리 늘어나지 않게 잡는 끈을 정합니다."},
            {"Collision", "천이 몸 충돌체(물리 에셋의 구와 캡슐)에 닿을 때를 정합니다."},
            {"Self Collision", "천이 자기 자신과 부딪히는 것을 정합니다. 반지름과 세기가 모두 0보다 커야 켜집니다."},
            {"Wind", "바람을 받는 방식과 정도를 정합니다. 바람 세기는 컴포넌트의 Cloth Wind Velocity로 줍니다."},
            {"Anim Drive", "칠한 입자를 애니메이션 위치로 당기는 힘을 정합니다."},
        };

        return Groups;
    }

    /**
     * @brief 바람 방식 선택 칸의 설명을 반환합니다.
     */
    const char* GetWindMethodTooltip() {
        return "Legacy: 입자마다 바람 속도 쪽으로 직접 가속을 줍니다.\n바람이 없어도 공기 저항처럼 작용해 천이 조금 느리게 움직입니다.\n\nAccurate: 천 면이 바람을 받는 방향을 보고 Drag와 Lift로 계산합니다.\n바람이 없으면 아무 힘도 주지 않습니다.";
    }

    /**
     * @brief 설정의 실수 항목 이름과 값 위치를 모읍니다.
     */
    Core::TArray<std::pair<Core::FString, float*>> GetConfigFloats(PhysicsRuntime::UClothConfigNv& Config) {
        Core::TArray<std::pair<Core::FString, float*>> Floats{};

        for (const FClothConfigField& Field : GetConfigFields()) {
            if (Field.mGetFloat != nullptr) {
                Floats.emplace_back(Field.mKey, Field.mGetFloat(Config));
            }
        }

        return Floats;
    }

    /**
     * @brief 설정의 벡터 항목 이름과 값 위치를 모읍니다.
     */
    Core::TArray<std::pair<Core::FString, FVector*>> GetConfigVectors(PhysicsRuntime::UClothConfigNv& Config) {
        Core::TArray<std::pair<Core::FString, FVector*>> Vectors{};

        for (const FClothConfigField& Field : GetConfigFields()) {
            if (Field.mGetVector != nullptr) {
                Vectors.emplace_back(Field.mKey, Field.mGetVector(Config));
            }
        }

        return Vectors;
    }

    /**
     * @brief 칠하기 대상의 이름, 단위, 범위, 설명을 반환합니다.
     */
    const FPaintTargetInfo& GetPaintTargetInfo(int TargetIndex) {
        static const FPaintTargetInfo Targets[NumPaintTargets]{
            {"Max Distance", "%.3f m", 1.0f, 0.002f, "입자가 애니메이션(스키닝) 위치에서 벗어날 수 있는 최대 거리입니다(m).\n0이면 입자가 몸에 고정되어 애니메이션만 따라갑니다.\n어깨, 허리처럼 붙어 있어야 할 곳은 0, 치맛자락처럼 날려야 할 곳은 크게 칠합니다."},
            {"Backstop Distance", "%.3f m", 1.0f, 0.002f, "입자가 몸 안쪽으로 파고들지 못하게 막는 구를 표면에서 몸 안쪽으로 얼마나 떨어뜨릴지입니다(m).\nBackstop Radius와 함께 칠해야 효과가 있습니다."},
            {"Backstop Radius", "%.3f m", 1.0f, 0.002f, "몸 안쪽을 막는 구의 반지름입니다(m).\n0이면 막지 않습니다. 입자는 이 구 안으로 들어가지 못합니다."},
            {"Anim Drive Multiplier", "%.2f", 1.0f, 0.005f, "입자를 애니메이션 위치로 당기는 세기의 배율입니다.\n0이면 당기지 않고, 1이면 설정의 Anim Drive Spring 세기 그대로 당깁니다."},
        };

        return Targets[std::clamp(TargetIndex, 0, NumPaintTargets - 1)];
    }

    /**
     * @brief 칠하기 대상 번호의 표시 이름을 반환합니다.
     */
    const char* GetPaintTargetName(int TargetIndex) {
        return GetPaintTargetInfo(TargetIndex).mName;
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
     * @brief 칠하기 도구의 이름과 설명을 반환합니다.
     */
    const Core::TArray<FClothLabel>& GetPaintTools() {
        static const Core::TArray<FClothLabel> Tools{
            {"Brush", "왼쪽 버튼으로 끄는 동안 붓 원 안의 입자에 Value를 칠합니다.\nShift를 누르고 끌면 0을 칠합니다."},
            {"Smooth", "왼쪽 버튼으로 끄는 동안 붓 원 안의 입자 값을 이웃 입자 값의 평균 쪽으로 다듬습니다.\n값이 계단처럼 끊기는 경계를 부드럽게 만들 때 씁니다."},
            {"Fill", "왼쪽 버튼으로 입자 하나를 누르면, 그 입자 값과 Threshold 이하로 차이 나는 이어진 입자를 모두 Value로 채웁니다.\n넓은 영역을 한 번에 칠할 때 씁니다."},
        };

        return Tools;
    }

    /**
     * @brief 시뮬레이션 메시의 삼각형으로 입자마다 변으로 이어진 이웃 입자를 모읍니다.
     */
    Core::TArray<Core::TArray<Uint32>> BuildParticleNeighbors(const PhysicsRuntime::FClothPhysicalMeshData& PhysMesh) {
        const Index_t NumParticles{PhysMesh.mVertices.size()};
        Core::TArray<Core::TArray<Uint32>> Neighbors(NumParticles);

        const auto AddNeighbor{[&Neighbors, NumParticles](Uint32 From, Uint32 To) {
            if (From < NumParticles && To < NumParticles && std::find(Neighbors[From].begin(), Neighbors[From].end(), To) == Neighbors[From].end()) {
                Neighbors[From].push_back(To);
            }
        }};

        for (Index_t BaseIndex{}; BaseIndex + 2 < PhysMesh.mIndices.size(); BaseIndex += 3) {
            for (Index_t Corner{}; Corner < 3; ++Corner) {
                const Uint32 A{PhysMesh.mIndices[BaseIndex + Corner]};
                const Uint32 B{PhysMesh.mIndices[BaseIndex + (Corner + 1) % 3]};

                AddNeighbor(A, B);
                AddNeighbor(B, A);
            }
        }

        return Neighbors;
    }

    /**
     * @brief 고른 입자의 값을 이웃 입자 값의 평균 쪽으로 다듬습니다.
     */
    void SmoothWeights(Core::TArray<float>& Values, const Core::TArray<Core::TArray<Uint32>>& Neighbors, const Core::TArray<Index_t>& Particles, float Strength) {
        // 다듬는 도중 바뀐 값을 다시 읽지 않도록 바꾸기 전 값으로 평균을 구합니다.
        const Core::TArray<float> Original{Values};
        const float Ratio{std::clamp(Strength, 0.0f, 1.0f)};

        for (const Index_t Particle : Particles) {
            if (Particle >= Values.size() || Particle >= Neighbors.size() || Neighbors[Particle].empty()) {
                continue;
            }

            float Sum{};

            for (const Uint32 Neighbor : Neighbors[Particle]) {
                Sum += Original[Neighbor];
            }

            const float Average{Sum / static_cast<float>(Neighbors[Particle].size())};

            Values[Particle] = Original[Particle] + (Average - Original[Particle]) * Ratio;
        }
    }

    /**
     * @brief 시작 입자에서 이웃을 따라가며 시작 값과 차이가 기준 이하인 입자를 한 값으로 채웁니다.
     */
    Index_t FillWeights(Core::TArray<float>& Values, const Core::TArray<Core::TArray<Uint32>>& Neighbors, Index_t StartParticle, float Value, float Threshold) {
        if (StartParticle >= Values.size() || StartParticle >= Neighbors.size()) {
            return 0;
        }

        // 바꾸기 전 시작 값과 비교해야 이미 채운 입자를 기준으로 번지지 않습니다.
        const float StartValue{Values[StartParticle]};
        Core::TArray<bool> Visited(Values.size(), false);
        Core::TArray<Index_t> Pending{StartParticle};
        Index_t NumFilled{};

        Visited[StartParticle] = true;

        while (!Pending.empty()) {
            const Index_t Particle{Pending.back()};
            Pending.pop_back();

            Values[Particle] = Value;
            ++NumFilled;

            for (const Uint32 Neighbor : Neighbors[Particle]) {
                if (!Visited[Neighbor] && std::abs(Values[Neighbor] - StartValue) <= Threshold) {
                    Visited[Neighbor] = true;
                    Pending.push_back(Neighbor);
                }
            }
        }

        return NumFilled;
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
}
