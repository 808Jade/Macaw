/**
 * @file FClothingSimulationNv.cpp
 * @brief NvCloth 솔버로 천 입자를 시뮬레이션하는 클래스를 구현합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FClothingSimulationNv.h"
#include "PhysicsRuntime/ClothingMeshUtils.h"
#include "PhysicsRuntime/FClothingSystemRuntimeModuleNv.h"
#include "PhysicsRuntime/PhysicsConversion.h"
#include "PhysicsRuntime/PhysicsStats.h"
#include "PhysicsRuntime/UClothingAssetCommon.h"
#include "Core/Log/Log.h"

#include <NvCloth/Cloth.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>
#include <NvClothExt/ClothFabricCooker.h>
#include <NvClothExt/ClothMeshDesc.h>
#include <NvClothExt/ClothMeshQuadifier.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace PhysicsRuntime {

    /**
     * @brief 천 시뮬레이션 로그 범주, 상수, 내부 함수입니다.
     */
    namespace {
        DEFINE_LOG_CATEGORY_STATIC(LogCloth, Log, All);

        /**
         * @brief 한 프레임에 진행할 최대 시간(초)입니다. 에디터가 멈췄다 풀릴 때 천이 튀지 않게 합니다.
         */
        constexpr float MaxDeltaTime{1.0f / 30.0f};

        /**
         * @brief 격자 테스트 천의 속도 감쇠 비율입니다. 흔들림이 시간이 지나면 멈추게 합니다.
         */
        constexpr float GridDamping{0.2f};

        /**
         * @brief 배열 전체를 NvCloth 읽기 범위로 바꿉니다.
         * @param[in] Values 가리킬 배열입니다.
         * @return 배열의 처음부터 끝까지를 가리키는 범위입니다.
         */
        template <typename T>
        nv::cloth::Range<const T> MakeRange(const Core::TArray<T>& Values) {
            return nv::cloth::Range<const T>{Values.data(), Values.data() + Values.size()};
        }

        /**
         * @brief 뼈 하나의 컴포넌트 좌표 변환을 구합니다.
         * @param[in] ComponentSpaceTransforms 스켈레톤 뼈 순서의 컴포넌트 좌표 뼈 행렬입니다.
         * @param[in] BoneIndex 뼈 번호입니다.
         * @return 뼈 변환이며 뼈가 없거나 변환으로 바꿀 수 없으면 단위 변환입니다.
         */
        FTransform GetBoneTransformCS(const Core::TArray<FMatrix>& ComponentSpaceTransforms, Index_t BoneIndex) {
            FTransform Result{};

            if (BoneIndex >= ComponentSpaceTransforms.size() || !ComponentSpaceTransforms[BoneIndex].TryGetTransform(Result)) {
                return FTransform{};
            }

            return Result;
        }

        /**
         * @brief 기준 뼈의 월드 변환을 천의 위치와 회전으로 넣습니다.
         * @param[in,out] Cloth 변환을 넣을 천입니다.
         * @param[in] ComponentToWorld 컴포넌트의 월드 행렬입니다.
         * @param[in] ComponentSpaceTransforms 스켈레톤 뼈 순서의 컴포넌트 좌표 뼈 행렬입니다.
         * @param[in] ReferenceBoneIndex 기준 뼈 번호입니다.
         */
        void SetSimRootTransform(nv::cloth::Cloth& Cloth, const FMatrix& ComponentToWorld, const Core::TArray<FMatrix>& ComponentSpaceTransforms, Index_t ReferenceBoneIndex) {
            const FMatrix SimRootMatrix{ReferenceBoneIndex < ComponentSpaceTransforms.size() ? ComponentSpaceTransforms[ReferenceBoneIndex] * ComponentToWorld : ComponentToWorld};
            FTransform SimRootTransform{};

            if (!SimRootMatrix.TryGetTransform(SimRootTransform)) {
                return;
            }

            Cloth.setTranslation(ToPxVec3(SimRootTransform.mLocation));
            Cloth.setRotation(ToPxQuat(SimRootTransform.mRotation));
        }

        /**
         * @brief 천 상세 단계 하나의 천과 패브릭을 해제합니다.
         * @param[in,out] LodData 해제할 데이터이며 주소를 비웁니다.
         */
        void ReleaseLodData(FClothingActorNv::FActorLodData& LodData) {
            delete LodData.mCloth;
            LodData.mCloth = nullptr;

            if (LodData.mFabric != nullptr) {
                LodData.mFabric->decRefCount();
                LodData.mFabric = nullptr;
            }
        }
    }

    /**
     * @brief 현재 뼈 포즈로 현재 천 상세 단계의 입자를 스키닝해 현재 스키닝 버퍼에 저장합니다.
     */
    void FClothingActorNv::SkinPhysicsMesh(const FClothingSimulationContextCommon& Context) {
        if (mAssetCreatedFrom == nullptr || mCurrentLodIndex >= mAssetCreatedFrom->mLodData.size()) {
            return;
        }

        const Core::FScopedStatTimer StatTimer{PhysicsStats::ClothSkinPhysMesh};
        const FClothPhysicalMeshData& PhysMesh{mAssetCreatedFrom->mLodData[mCurrentLodIndex].mPhysicalMeshData};
        const FTransform RootBoneTransform{GetBoneTransformCS(Context.mBoneTransforms, mAssetCreatedFrom->mReferenceBoneIndex)};

        ClothingMeshUtils::SkinPhysicsMesh(mAssetCreatedFrom->mUsedBoneIndices, PhysMesh, RootBoneTransform, Context.mRefToLocals, mSkinnedPhysicsMeshPositions[mCurrentSkinnedPositionIndex], mSkinnedPhysicsMeshNormals);
    }

    /**
     * @brief 현재 스키닝 위치로 모션 제약과 분리 제약을 갱신합니다.
     */
    void FClothingActorNv::UpdateMotionConstraints(const FClothingSimulationContextCommon& Context) {
        if (mAssetCreatedFrom == nullptr || mCurrentLodIndex >= mLodData.size() || mCurrentLodIndex >= mAssetCreatedFrom->mLodData.size() || mLodData[mCurrentLodIndex].mCloth == nullptr) {
            return;
        }

        const Core::TArray<FVector>& CurrentSkinnedPositions{GetCurrentSkinnedPositions()};
        nv::cloth::Cloth& CurrentCloth{*mLodData[mCurrentLodIndex].mCloth};
        const FClothPhysicalMeshData& PhysMesh{mAssetCreatedFrom->mLodData[mCurrentLodIndex].mPhysicalMeshData};

        // 입자마다 이번 프레임 스키닝 위치를 중심으로 하는 구를 주어 그 밖으로 나가지 못하게 합니다.
        nv::cloth::Range<physx::PxVec4> MotionConstraints{CurrentCloth.getMotionConstraints()};
        const Index_t NumMotionConstraints{std::min<Index_t>(CurrentCloth.getNumMotionConstraints(), CurrentSkinnedPositions.size())};
        const FPointWeightMap* const MaxDistances{PhysMesh.FindWeightMap(EWeightMapTargetCommon::MaxDistance)};

        if (MaxDistances != nullptr && MaxDistances->Num() > 0 && MaxDistances->Num() >= NumMotionConstraints) {
            for (Index_t ConstraintIndex{}; ConstraintIndex < NumMotionConstraints; ++ConstraintIndex) {
                MotionConstraints[static_cast<Uint32>(ConstraintIndex)] = physx::PxVec4{ToPxVec3(CurrentSkinnedPositions[ConstraintIndex]), (*MaxDistances)[ConstraintIndex] * Context.mMaxDistanceScale};
            }
        } else {
            // 최대 거리를 칠하지 않았으면 반지름을 무한으로 두어 제한하지 않습니다.
            for (Index_t ConstraintIndex{}; ConstraintIndex < NumMotionConstraints; ++ConstraintIndex) {
                MotionConstraints[static_cast<Uint32>(ConstraintIndex)] = physx::PxVec4{ToPxVec3(CurrentSkinnedPositions[ConstraintIndex]), std::numeric_limits<float>::max()};
            }
        }

        // 뒤쪽 거리와 반지름을 칠했으면 법선 반대쪽에 구를 두어 입자가 몸 안으로 파고들지 못하게 합니다.
        const FPointWeightMap* const BackstopDistances{PhysMesh.FindWeightMap(EWeightMapTargetCommon::BackstopDistance)};
        const FPointWeightMap* const BackstopRadiuses{PhysMesh.FindWeightMap(EWeightMapTargetCommon::BackstopRadius)};

        if (BackstopDistances != nullptr && BackstopDistances->Num() > 0 && BackstopRadiuses != nullptr && BackstopRadiuses->Num() > 0) {
            nv::cloth::Range<physx::PxVec4> SeparationConstraints{CurrentCloth.getSeparationConstraints()};
            const Index_t NumSeparationConstraints{std::min({static_cast<Index_t>(CurrentCloth.getNumSeparationConstraints()), CurrentSkinnedPositions.size(), mSkinnedPhysicsMeshNormals.size(), BackstopDistances->Num(), BackstopRadiuses->Num()})};

            for (Index_t ConstraintIndex{}; ConstraintIndex < NumSeparationConstraints; ++ConstraintIndex) {
                const FVector Center{CurrentSkinnedPositions[ConstraintIndex] - mSkinnedPhysicsMeshNormals[ConstraintIndex] * (*BackstopDistances)[ConstraintIndex]};

                SeparationConstraints[static_cast<Uint32>(ConstraintIndex)] = physx::PxVec4{ToPxVec3(Center), (*BackstopRadiuses)[ConstraintIndex]};
            }
        }
    }

    /**
     * @brief 이번 프레임에 스키닝한 입자 위치를 반환합니다.
     */
    const Core::TArray<FVector>& FClothingActorNv::GetCurrentSkinnedPositions() const {
        return mSkinnedPhysicsMeshPositions[mCurrentSkinnedPositionIndex];
    }

    /**
     * @brief 지난 프레임에 스키닝한 입자 위치를 반환합니다.
     */
    const Core::TArray<FVector>& FClothingActorNv::GetPreviousSkinnedPositions() const {
        return mSkinnedPhysicsMeshPositions[(mCurrentSkinnedPositionIndex + 1) % 2];
    }

    /**
     * @brief 솔버가 없는 빈 시뮬레이션을 구성합니다.
     */
    FClothingSimulationNv::FClothingSimulationNv() = default;

    /**
     * @brief 등록한 천과 솔버를 해제합니다.
     */
    FClothingSimulationNv::~FClothingSimulationNv() {
        Shutdown();
    }

    /**
     * @brief 천 시스템 모듈의 팩토리로 솔버를 만듭니다.
     */
    bool FClothingSimulationNv::Initialize(const FClothingSystemRuntimeModuleNv& Module) {
        if (mSolver != nullptr) {
            return true;
        }

        if (Module.GetSoftwareFactory() == nullptr || Module.GetMeshQuadifier() == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot initialize cloth simulation without a factory.");

            return false;
        }

        mFactory = Module.GetSoftwareFactory();
        mMeshQuadifier = Module.GetMeshQuadifier();
        mSolver = mFactory->createSolver();

        if (mSolver == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot create the NvCloth solver.");
            mFactory = nullptr;
            mMeshQuadifier = nullptr;

            return false;
        }

        return true;
    }

    /**
     * @brief 등록한 천을 모두 해제한 뒤 솔버를 해제합니다.
     */
    void FClothingSimulationNv::Shutdown() {
        DestroyActors();

        delete mSolver;
        mSolver = nullptr;
        mFactory = nullptr;
        mMeshQuadifier = nullptr;
    }

    /**
     * @brief 한 변을 고정한 격자 천을 만들어 솔버에 등록합니다.
     */
    bool FClothingSimulationNv::CreateGridActor(const FVector& Origin, Uint32 Width, Uint32 Height, float Spacing) {
        if (mSolver == nullptr || Width < 2 || Height < 2 || !(Spacing > 0.0f)) {
            return false;
        }

        DestroyActors();

        const Uint32 NumParticles{Width * Height};
        Core::TArray<physx::PxVec3> Points{};
        Core::TArray<float> InvMasses{};
        Core::TArray<Uint32> Triangles{};

        Points.reserve(NumParticles);
        InvMasses.reserve(NumParticles);
        Triangles.reserve(static_cast<Index_t>(Width - 1) * (Height - 1) * 6);

        for (Uint32 Row{}; Row < Height; ++Row) {
            for (Uint32 Column{}; Column < Width; ++Column) {
                Points.push_back(ToPxVec3(Origin + FVector{static_cast<float>(Column) * Spacing, static_cast<float>(Row) * Spacing, 0.0f}));
                InvMasses.push_back(Row == 0 ? 0.0f : 1.0f);
            }
        }

        for (Uint32 Row{}; Row + 1 < Height; ++Row) {
            for (Uint32 Column{}; Column + 1 < Width; ++Column) {
                const Uint32 Corner{Row * Width + Column};

                Triangles.insert(Triangles.end(), {Corner, Corner + Width, Corner + 1});
                Triangles.insert(Triangles.end(), {Corner + 1, Corner + Width, Corner + Width + 1});
            }
        }

        nv::cloth::ClothMeshDesc MeshDesc{};
        MeshDesc.points.data = Points.data();
        MeshDesc.points.count = NumParticles;
        MeshDesc.points.stride = sizeof(physx::PxVec3);
        MeshDesc.invMasses.data = InvMasses.data();
        MeshDesc.invMasses.count = NumParticles;
        MeshDesc.invMasses.stride = sizeof(float);
        MeshDesc.triangles.data = Triangles.data();
        MeshDesc.triangles.count = static_cast<Uint32>(Triangles.size() / 3);
        MeshDesc.triangles.stride = sizeof(Uint32) * 3;

        nv::cloth::Vector<int32_t>::Type PhaseTypes{};
        nv::cloth::Fabric* Fabric{NvClothCookFabricFromMesh(mFactory, MeshDesc, ToPxVec3(mGravity.Normalized()), &PhaseTypes, true)};

        if (Fabric == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot cook a cloth fabric for the {}x{} grid.", Width, Height);

            return false;
        }

        FClothingActorNv::FActorLodData LodData{};
        LodData.mFabric = Fabric;
        LodData.mRestPositions.reserve(NumParticles);

        for (Uint32 Index{}; Index < NumParticles; ++Index) {
            LodData.mRestPositions.emplace_back(Points[Index], InvMasses[Index]);
        }

        LodData.mCloth = mFactory->createCloth(MakeRange(LodData.mRestPositions), *Fabric);

        if (LodData.mCloth == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot create cloth for the {}x{} grid.", Width, Height);
            ReleaseLodData(LodData);

            return false;
        }

        // 위상마다 설정을 넣어야 솔버가 그 위상의 거리 제약을 풉니다.
        LodData.mPhaseConfigs.resize(Fabric->getNumPhases());

        for (Index_t PhaseIndex{}; PhaseIndex < LodData.mPhaseConfigs.size(); ++PhaseIndex) {
            LodData.mPhaseConfigs[PhaseIndex].mPhaseIndex = static_cast<uint16_t>(PhaseIndex);
            LodData.mPhaseTypes.push_back(PhaseTypes[static_cast<Uint32>(PhaseIndex)]);
        }

        LodData.mCloth->setPhaseConfig(MakeRange(LodData.mPhaseConfigs));
        LodData.mCloth->setDamping(physx::PxVec3{GridDamping, GridDamping, GridDamping});

        mSolver->addCloth(LodData.mCloth);

        FClothingActorNv& NewActor{mActors.emplace_back()};
        NewActor.mLodData.push_back(std::move(LodData));
        NewActor.mCurrentLodIndex = 0;
        mGridWidth = Width;
        mGridHeight = Height;

        MACAW_LOG(LogCloth, Log, "Created a {}x{} grid cloth with {} phases.", Width, Height, NewActor.mLodData[0].mPhaseConfigs.size());

        return true;
    }

    /**
     * @brief 천 에셋의 상세 단계마다 NvCloth 천을 만들고 현재 상세 단계의 천을 솔버에 넣습니다.
     */
    bool FClothingSimulationNv::CreateActor(const UClothingAssetCommon& Asset, Index_t SimDataIndex, const FMatrix& ComponentToWorld, const Core::TArray<FMatrix>& ComponentSpaceTransforms, const Core::TArray<FMatrix>& RefToLocals) {
        if (mSolver == nullptr || mMeshQuadifier == nullptr) {
            return false;
        }

        FClothingActorNv NewActor{};
        NewActor.mSimDataIndex = SimDataIndex;
        NewActor.mAssetCreatedFrom = &Asset;

        // 이미 자세를 취한 메시에서도 천이 제자리에 생기도록 입자를 현재 자세로 스키닝합니다.
        const FTransform SimBoneTransformCS{GetBoneTransformCS(ComponentSpaceTransforms, Asset.mReferenceBoneIndex)};

        for (Index_t LodIndex{}; LodIndex < Asset.mLodData.size(); ++LodIndex) {
            const FClothPhysicalMeshData& PhysMesh{Asset.mLodData[LodIndex].mPhysicalMeshData};
            const Index_t NumVerts{PhysMesh.mVertices.size()};
            const Index_t NumTriangles{PhysMesh.mIndices.size() / 3};

            if (NumVerts == 0 || NumTriangles == 0 || PhysMesh.mInverseMasses.size() != NumVerts) {
                MACAW_LOG(LogCloth, Error, "Cannot create cloth LOD{}. The sim mesh is empty.", LodIndex);

                for (FClothingActorNv::FActorLodData& CreatedLod : NewActor.mLodData) {
                    ReleaseLodData(CreatedLod);
                }

                return false;
            }

            Core::TArray<FVector> SkinnedVerts{};
            Core::TArray<FVector> SkinnedNormals{};
            ClothingMeshUtils::SkinPhysicsMesh(Asset.mUsedBoneIndices, PhysMesh, SimBoneTransformCS, RefToLocals, SkinnedVerts, SkinnedNormals);

            Core::TArray<physx::PxVec3> Points{};
            Points.reserve(NumVerts);

            for (const FVector& SkinnedVert : SkinnedVerts) {
                Points.push_back(ToPxVec3(SkinnedVert));
            }

            nv::cloth::ClothMeshDesc MeshDesc{};
            MeshDesc.points.data = Points.data();
            MeshDesc.points.count = static_cast<Uint32>(NumVerts);
            MeshDesc.points.stride = sizeof(physx::PxVec3);
            MeshDesc.triangles.data = PhysMesh.mIndices.data();
            MeshDesc.triangles.count = static_cast<Uint32>(NumTriangles);
            MeshDesc.triangles.stride = sizeof(Uint32) * 3;

            // 모든 입자가 고정이면 제약을 만들 수 없으므로 그때는 역질량을 넘기지 않습니다.
            const bool HasValidMasses{std::any_of(PhysMesh.mInverseMasses.begin(), PhysMesh.mInverseMasses.end(), [](float InvMass) {
                return InvMass > 0.0f;
            })};

            if (HasValidMasses) {
                MeshDesc.invMasses.data = PhysMesh.mInverseMasses.data();
                MeshDesc.invMasses.count = static_cast<Uint32>(NumVerts);
                MeshDesc.invMasses.stride = sizeof(float);
            }

            // NvCloth는 사각형 메시에서 제약을 더 고르게 만들므로 삼각형을 사각형으로 합친 뒤 굽습니다.
            mMeshQuadifier->quadify(MeshDesc);

            nv::cloth::Vector<int32_t>::Type PhaseTypes{};
            nv::cloth::Fabric* Fabric{NvClothCookFabricFromMesh(mFactory, mMeshQuadifier->getDescriptor(), ToPxVec3(mGravity.Normalized()), &PhaseTypes, true)};

            if (Fabric == nullptr) {
                MACAW_LOG(LogCloth, Error, "Cannot cook a cloth fabric for LOD{}.", LodIndex);

                for (FClothingActorNv::FActorLodData& CreatedLod : NewActor.mLodData) {
                    ReleaseLodData(CreatedLod);
                }

                return false;
            }

            FClothingActorNv::FActorLodData ActorLodData{};
            ActorLodData.mFabric = Fabric;
            ActorLodData.mRestPositions.reserve(NumVerts);

            for (Index_t VertIndex{}; VertIndex < NumVerts; ++VertIndex) {
                ActorLodData.mRestPositions.emplace_back(Points[VertIndex], PhysMesh.mInverseMasses[VertIndex]);
            }

            ActorLodData.mCloth = mFactory->createCloth(MakeRange(ActorLodData.mRestPositions), *Fabric);

            if (ActorLodData.mCloth == nullptr) {
                MACAW_LOG(LogCloth, Error, "Cannot create cloth for LOD{}.", LodIndex);
                ReleaseLodData(ActorLodData);

                for (FClothingActorNv::FActorLodData& CreatedLod : NewActor.mLodData) {
                    ReleaseLodData(CreatedLod);
                }

                return false;
            }

            // 위상마다 번호를 넣어 둡니다. 세기 같은 값은 천 설정에서 채웁니다.
            const Uint32 NumPhases{Fabric->getNumPhases()};
            ActorLodData.mPhaseConfigs.resize(NumPhases);
            ActorLodData.mPhaseTypes.resize(NumPhases);

            for (Uint32 PhaseIndex{}; PhaseIndex < NumPhases; ++PhaseIndex) {
                ActorLodData.mPhaseTypes[PhaseIndex] = PhaseTypes[PhaseIndex];
                ActorLodData.mPhaseConfigs[PhaseIndex].mPhaseIndex = static_cast<uint16_t>(PhaseIndex);
            }

            ActorLodData.mCloth->setPhaseConfig(MakeRange(ActorLodData.mPhaseConfigs));

            // 입자마다 스키닝 위치를 중심, 최대 거리를 반지름으로 하는 구 안에서만 움직이게 합니다.
            const FPointWeightMap* const MaxDistances{PhysMesh.FindWeightMap(EWeightMapTargetCommon::MaxDistance)};
            const bool HasMaxDistances{MaxDistances != nullptr && MaxDistances->Num() == NumVerts};
            nv::cloth::Range<physx::PxVec4> MotionConstraints{ActorLodData.mCloth->getMotionConstraints()};
            const Index_t NumMotionConstraints{std::min<Index_t>(ActorLodData.mCloth->getNumMotionConstraints(), NumVerts)};

            for (Index_t ConstraintIndex{}; ConstraintIndex < NumMotionConstraints; ++ConstraintIndex) {
                const float Radius{HasMaxDistances ? (*MaxDistances)[ConstraintIndex] : std::numeric_limits<float>::max()};

                MotionConstraints[static_cast<Uint32>(ConstraintIndex)] = physx::PxVec4{Points[ConstraintIndex], Radius};
            }

            // 첫 프레임에 원점에서 순간 이동한 것으로 보지 않도록 변환을 넣고 관성을 비웁니다.
            SetSimRootTransform(*ActorLodData.mCloth, ComponentToWorld, ComponentSpaceTransforms, Asset.mReferenceBoneIndex);
            ActorLodData.mCloth->clearInertia();

            // 첫 프레임은 상세 단계 0이 맡으므로 그 스키닝 결과로 두 버퍼를 채워 지난 프레임 값도 있게 합니다.
            if (LodIndex == 0) {
                NewActor.mSkinnedPhysicsMeshPositions[0] = SkinnedVerts;
                NewActor.mSkinnedPhysicsMeshPositions[1] = SkinnedVerts;
                NewActor.mSkinnedPhysicsMeshNormals = SkinnedNormals;
            }

            NewActor.mLodData.push_back(std::move(ActorLodData));
        }

        if (NewActor.mLodData.empty()) {
            return false;
        }

        mActors.push_back(std::move(NewActor));

        // 새 천이 현재 상세 단계로 솔버에 들어가도록 모든 천을 다시 검사합니다.
        UpdateLod(mCurrentMeshLodIndex == InvalidIndex ? 0 : mCurrentMeshLodIndex, ComponentToWorld, ComponentSpaceTransforms, RefToLocals, true, true);

        MACAW_LOG(LogCloth, Log, "Created a cloth actor with {} LODs and {} particles.", mActors.back().mLodData.size(), Asset.mLodData[0].mPhysicalMeshData.mVertices.size());

        return true;
    }

    /**
     * @brief 입력의 포즈와 변환으로 천 에셋의 천을 만들어 솔버에 넣습니다.
     */
    bool FClothingSimulationNv::CreateActor(const UClothingAssetCommon& Asset, Index_t SimDataIndex, const FClothingSimulationContextCommon& Context) {
        if (!CreateActor(Asset, SimDataIndex, Context.mComponentToWorld, Context.mBoneTransforms, Context.mRefToLocals)) {
            return false;
        }

        // 결과를 처음 꺼낼 때도 법선이 있도록 만든 직후의 입자로 법선을 구합니다.
        ComputePhysicalMeshNormals(mActors.back());

        return true;
    }

    /**
     * @brief 메시 상세 단계에 맞는 천 상세 단계를 골라 솔버의 천을 바꿉니다.
     */
    void FClothingSimulationNv::UpdateLod(Index_t PredictedLod, const FMatrix& ComponentToWorld, const Core::TArray<FMatrix>& ComponentSpaceTransforms, const Core::TArray<FMatrix>& RefToLocals, bool ForceNoRemap, bool ForceActorChecks) {
        if (mSolver == nullptr || (PredictedLod == mCurrentMeshLodIndex && !ForceActorChecks)) {
            return;
        }

        for (FClothingActorNv& Actor : mActors) {
            if (Actor.mAssetCreatedFrom == nullptr) {
                continue;
            }

            const UClothingAssetCommon& Asset{*Actor.mAssetCreatedFrom};
            const Core::TArray<Index_t>& LodMap{Asset.mLodMap};

            // 메시 상세 단계에 천이 없으면 지금 천을 솔버에서 빼고 넘어갑니다.
            if (PredictedLod >= LodMap.size()) {
                if (Actor.mCurrentLodIndex != InvalidIndex) {
                    mSolver->removeCloth(Actor.mLodData[Actor.mCurrentLodIndex].mCloth);
                }

                Actor.mCurrentLodIndex = InvalidIndex;

                continue;
            }

            const bool OldLodMapped{mCurrentMeshLodIndex < LodMap.size() && LodMap[mCurrentMeshLodIndex] != InvalidIndex};

            // 메시 상세 단계에 연결된 천 상세 단계가 없으면 더 낮은 단계에서 찾습니다.
            Index_t PredictedClothingLod{InvalidIndex};

            for (Index_t PotentialLod{PredictedLod}; PotentialLod < LodMap.size(); ++PotentialLod) {
                if (LodMap[PotentialLod] != InvalidIndex) {
                    PredictedClothingLod = LodMap[PotentialLod];

                    break;
                }
            }

            if (PredictedClothingLod == Actor.mCurrentLodIndex) {
                continue;
            }

            if (PredictedClothingLod >= Actor.mLodData.size()) {
                if (Actor.mCurrentLodIndex != InvalidIndex) {
                    mSolver->removeCloth(Actor.mLodData[Actor.mCurrentLodIndex].mCloth);
                }

                Actor.mCurrentLodIndex = InvalidIndex;

                continue;
            }

            FClothingActorNv::FActorLodData& NewLodData{Actor.mLodData[PredictedClothingLod]};
            nv::cloth::Cloth& NewCloth{*NewLodData.mCloth};

            if (OldLodMapped && !ForceNoRemap && Actor.mCurrentLodIndex != InvalidIndex) {
                // 상세 단계를 바꿉니다. 메시 상세 단계가 하나뿐이라 이전 입자를 옮기지 않고 처음 상태에서 다시 시작합니다.
                mSolver->removeCloth(Actor.mLodData[Actor.mCurrentLodIndex].mCloth);
                mSolver->addCloth(&NewCloth);

                nv::cloth::MappedRange<physx::PxVec4> NewParticles{NewCloth.getCurrentParticles()};
                nv::cloth::MappedRange<physx::PxVec4> NewPrevParticles{NewCloth.getPreviousParticles()};

                for (Uint32 ParticleIndex{}; ParticleIndex < NewParticles.size(); ++ParticleIndex) {
                    NewParticles[ParticleIndex] = NewLodData.mRestPositions[ParticleIndex];
                    NewPrevParticles[ParticleIndex] = NewLodData.mRestPositions[ParticleIndex];
                }

                NewCloth.clearParticleAccelerations();
            } else {
                mSolver->addCloth(&NewCloth);

                nv::cloth::MappedRange<physx::PxVec4> NewParticles{NewCloth.getCurrentParticles()};
                nv::cloth::MappedRange<physx::PxVec4> NewPrevParticles{NewCloth.getPreviousParticles()};

                if (Asset.mReferenceBoneIndex < ComponentSpaceTransforms.size()) {
                    // 이전 상세 단계가 없으므로 입자를 현재 자세의 스키닝 위치에 놓습니다.
                    Core::TArray<FVector> SkinnedPositions{};
                    Core::TArray<FVector> SkinnedNormals{};
                    ClothingMeshUtils::SkinPhysicsMesh(Asset.mUsedBoneIndices, Asset.mLodData[PredictedClothingLod].mPhysicalMeshData, GetBoneTransformCS(ComponentSpaceTransforms, Asset.mReferenceBoneIndex), RefToLocals, SkinnedPositions, SkinnedNormals);

                    for (Uint32 ParticleIndex{}; ParticleIndex < NewParticles.size() && ParticleIndex < SkinnedPositions.size(); ++ParticleIndex) {
                        NewParticles[ParticleIndex] = physx::PxVec4{ToPxVec3(SkinnedPositions[ParticleIndex]), NewParticles[ParticleIndex].w};
                        NewPrevParticles[ParticleIndex] = physx::PxVec4{ToPxVec3(SkinnedPositions[ParticleIndex]), NewPrevParticles[ParticleIndex].w};
                    }
                } else {
                    for (Uint32 ParticleIndex{}; ParticleIndex < NewParticles.size(); ++ParticleIndex) {
                        NewParticles[ParticleIndex] = NewLodData.mRestPositions[ParticleIndex];
                        NewPrevParticles[ParticleIndex] = NewLodData.mRestPositions[ParticleIndex];
                    }
                }

                NewCloth.clearParticleAccelerations();
            }

            SetSimRootTransform(NewCloth, ComponentToWorld, ComponentSpaceTransforms, Asset.mReferenceBoneIndex);
            NewCloth.clearInertia();

            // 이전에 시뮬레이션하던 충돌체와 제약이 남지 않게 비웁니다.
            NewCloth.setSpheres(nv::cloth::Range<const physx::PxVec4>{}, 0, NewCloth.getNumSpheres());
            NewCloth.setPlanes(nv::cloth::Range<const physx::PxVec4>{}, 0, NewCloth.getNumPlanes());
            NewCloth.clearMotionConstraints();
            NewCloth.clearSeparationConstraints();

            Actor.mCurrentLodIndex = PredictedClothingLod;
        }

        mCurrentMeshLodIndex = PredictedLod;
    }

    /**
     * @brief 등록한 천을 솔버에서 빼고 천과 패브릭을 해제합니다.
     */
    void FClothingSimulationNv::DestroyActors() {
        for (FClothingActorNv& Actor : mActors) {
            if (mSolver != nullptr && Actor.mCurrentLodIndex < Actor.mLodData.size()) {
                mSolver->removeCloth(Actor.mLodData[Actor.mCurrentLodIndex].mCloth);
            }

            for (FClothingActorNv::FActorLodData& LodData : Actor.mLodData) {
                ReleaseLodData(LodData);
            }
        }

        mActors.clear();
        mCurrentMeshLodIndex = InvalidIndex;
        mGridWidth = 0;
        mGridHeight = 0;
    }

    /**
     * @brief 모든 천에 중력을 적용하고 솔버를 한 프레임 진행합니다.
     */
    void FClothingSimulationNv::Simulate(float DeltaTime) {
        if (mSolver == nullptr || mActors.empty() || !(DeltaTime > 0.0f)) {
            return;
        }

        const Core::FScopedStatTimer StatTimer{PhysicsStats::ClothSimulate};
        Index_t NumParticles{};

        for (const FClothingActorNv& Actor : mActors) {
            if (Actor.mCurrentLodIndex >= Actor.mLodData.size()) {
                continue;
            }

            nv::cloth::Cloth& Cloth{*Actor.mLodData[Actor.mCurrentLodIndex].mCloth};
            Cloth.setGravity(ToPxVec3(mGravity));
            NumParticles += Cloth.getNumParticles();
        }

        if (mSolver->beginSimulation(std::min(DeltaTime, MaxDeltaTime))) {
            const int ChunkCount{mSolver->getSimulationChunkCount()};

            for (int ChunkIndex{}; ChunkIndex < ChunkCount; ++ChunkIndex) {
                mSolver->simulateChunk(ChunkIndex);
            }

            mSolver->endSimulation();
        }

        PhysicsStats::ClothParticles.Set(static_cast<double>(NumParticles));
    }

    /**
     * @brief 입력으로 천 에셋의 천을 한 프레임 진행합니다.
     */
    void FClothingSimulationNv::Simulate(const FClothingSimulationContextCommon& Context) {
        if (mSolver == nullptr || mActors.empty() || Context.mBoneTransforms.empty() || !(Context.mDeltaSeconds > 0.0f)) {
            return;
        }

        const Core::FScopedStatTimer StatTimer{PhysicsStats::ClothSimulate};
        Index_t NumParticles{};

        UpdateLod(Context.mPredictedLod, Context.mComponentToWorld, Context.mBoneTransforms, Context.mRefToLocals, false, false);

        for (FClothingActorNv& Actor : mActors) {
            if (Actor.mAssetCreatedFrom == nullptr || Actor.mCurrentLodIndex >= Actor.mLodData.size()) {
                continue;
            }

            // 모션 제약을 만들려면 시뮬레이션 메시 전체를 이번 포즈로 스키닝해야 합니다.
            Actor.SkinPhysicsMesh(Context);

            nv::cloth::Cloth& Cloth{*Actor.mLodData[Actor.mCurrentLodIndex].mCloth};

            // 기준 뼈의 월드 변환이 지난 프레임과 달라진 만큼 NvCloth가 입자에 관성을 줍니다.
            SetSimRootTransform(Cloth, Context.mComponentToWorld, Context.mBoneTransforms, Actor.mAssetCreatedFrom->mReferenceBoneIndex);
            Cloth.setGravity(ToPxVec3(Context.mWorldGravity));

            // 최대 거리가 0인 입자는 스키닝 위치에 붙어 애니메이션을 따라가고, 나머지는 그 거리 안에서만 흔들립니다.
            Actor.UpdateMotionConstraints(Context);

            NumParticles += Cloth.getNumParticles();
        }

        if (mSolver->beginSimulation(Context.mDeltaSeconds)) {
            const int ChunkCount{mSolver->getSimulationChunkCount()};

            for (int ChunkIndex{}; ChunkIndex < ChunkCount; ++ChunkIndex) {
                mSolver->simulateChunk(ChunkIndex);
            }

            mSolver->endSimulation();
        }

        for (FClothingActorNv& Actor : mActors) {
            if (Actor.mAssetCreatedFrom == nullptr || Actor.mCurrentLodIndex >= Actor.mLodData.size()) {
                continue;
            }

            ComputePhysicalMeshNormals(Actor);

            // 다음 프레임 스키닝은 다른 버퍼에 써서 이번 프레임 값을 지난 프레임 값으로 남깁니다.
            Actor.mCurrentSkinnedPositionIndex = (Actor.mCurrentSkinnedPositionIndex + 1) % 2;
        }

        PhysicsStats::ClothParticles.Set(static_cast<double>(NumParticles));
    }

    /**
     * @brief 천마다 현재 입자 위치와 법선, 기준 뼈 변환을 복사합니다.
     */
    void FClothingSimulationNv::GetSimulationData(Core::TMap<Index_t, FClothSimulData>& OutData, const FClothingSimulationContextCommon& Context) const {
        if (mActors.empty()) {
            OutData.clear();

            return;
        }

        for (const FClothingActorNv& Actor : mActors) {
            if (Actor.mAssetCreatedFrom == nullptr || Actor.mCurrentLodIndex >= Actor.mLodData.size()) {
                continue;
            }

            const Index_t ReferenceBoneIndex{Actor.mAssetCreatedFrom->mReferenceBoneIndex};
            FClothSimulData& ClothData{OutData[Actor.mSimDataIndex]};

            if (ReferenceBoneIndex >= Context.mBoneTransforms.size()) {
                MACAW_LOG(LogCloth, Warning, "Failed to write back cloth simulation data because the bone transforms are invalid.");
                ClothData.Reset();

                return;
            }

            // 입자는 크기를 뺀 기준 뼈 좌표이므로 크기를 뺀 기준 뼈 변환에 컴포넌트 월드 변환을 곱해 기준 변환을 만듭니다.
            FTransform RootBoneTransform{GetBoneTransformCS(Context.mBoneTransforms, ReferenceBoneIndex)};
            RootBoneTransform.mScale = FVector{1.0f, 1.0f, 1.0f};

            FTransform RootBoneWorldTransform{};

            if (!(RootBoneTransform.ToMatrix() * Context.mComponentToWorld).TryGetTransform(RootBoneWorldTransform)) {
                ClothData.Reset();

                continue;
            }

            const nv::cloth::Cloth& Cloth{*Actor.mLodData[Actor.mCurrentLodIndex].mCloth};
            const nv::cloth::MappedRange<const physx::PxVec4> Particles{Cloth.getCurrentParticles()};

            ClothData.Reset();
            ClothData.mTransform = RootBoneWorldTransform;
            ClothData.mComponentRelativeTransform = RootBoneTransform;
            ClothData.mPositions.reserve(Particles.size());

            for (const physx::PxVec4& Particle : Particles) {
                ClothData.mPositions.push_back(ToFVector(Particle.getXYZ()));
            }

            ClothData.mNormals = Actor.mCurrentNormals;
        }
    }

    /**
     * @brief 솔버에 들어간 천이 있는지 검사합니다.
     */
    bool FClothingSimulationNv::ShouldSimulate() const {
        return std::any_of(mActors.begin(), mActors.end(), [](const FClothingActorNv& Actor) {
            return Actor.mCurrentLodIndex < Actor.mLodData.size();
        });
    }

    /**
     * @brief 천의 현재 입자 위치로 삼각형 법선을 더해 입자 법선을 구합니다.
     */
    void FClothingSimulationNv::ComputePhysicalMeshNormals(FClothingActorNv& Actor) const {
        if (Actor.mAssetCreatedFrom == nullptr || Actor.mCurrentLodIndex >= Actor.mLodData.size()) {
            return;
        }

        const nv::cloth::Cloth& Cloth{*Actor.mLodData[Actor.mCurrentLodIndex].mCloth};
        const nv::cloth::MappedRange<const physx::PxVec4> Particles{Cloth.getCurrentParticles()};
        const Core::TArray<Uint32>& Indices{Actor.mAssetCreatedFrom->mLodData[Actor.mCurrentLodIndex].mPhysicalMeshData.mIndices};

        Actor.mCurrentNormals.assign(Particles.size(), FVector{0.0f, 0.0f, 0.0f});

        // 입자가 닿은 삼각형마다 넓이만큼 큰 면 법선을 더한 뒤 마지막에 정규화합니다.
        for (Index_t BaseIndex{}; BaseIndex + 2 < Indices.size(); BaseIndex += 3) {
            const Uint32 IndexA{Indices[BaseIndex]};
            const Uint32 IndexB{Indices[BaseIndex + 1]};
            const Uint32 IndexC{Indices[BaseIndex + 2]};

            if (IndexA >= Particles.size() || IndexB >= Particles.size() || IndexC >= Particles.size()) {
                continue;
            }

            const FVector A{ToFVector(Particles[IndexA].getXYZ())};
            const FVector B{ToFVector(Particles[IndexB].getXYZ())};
            const FVector C{ToFVector(Particles[IndexC].getXYZ())};
            const FVector TriangleNormal{(B - A).Cross(C - A)};

            Actor.mCurrentNormals[IndexA] += TriangleNormal;
            Actor.mCurrentNormals[IndexB] += TriangleNormal;
            Actor.mCurrentNormals[IndexC] += TriangleNormal;
        }

        for (FVector& Normal : Actor.mCurrentNormals) {
            Normal.Normalize();
        }
    }

    /**
     * @brief 천 하나의 현재 입자 위치를 복사합니다.
     */
    bool FClothingSimulationNv::GetParticles(Index_t ActorIndex, Core::TArray<FVector>& OutPositions) const {
        OutPositions.clear();

        if (ActorIndex >= mActors.size() || mActors[ActorIndex].mCurrentLodIndex >= mActors[ActorIndex].mLodData.size()) {
            return false;
        }

        const FClothingActorNv& Actor{mActors[ActorIndex]};
        const nv::cloth::Cloth& Cloth{*Actor.mLodData[Actor.mCurrentLodIndex].mCloth};
        const nv::cloth::MappedRange<const physx::PxVec4> Particles{Cloth.getCurrentParticles()};

        OutPositions.reserve(Particles.size());

        for (const physx::PxVec4& Particle : Particles) {
            OutPositions.push_back(ToFVector(Particle.getXYZ()));
        }

        return true;
    }

    /**
     * @brief 등록한 천의 개수를 반환합니다.
     */
    Index_t FClothingSimulationNv::GetNumActors() const {
        return mActors.size();
    }

    /**
     * @brief 마지막으로 만든 격자의 X 방향 입자 수를 반환합니다.
     */
    Uint32 FClothingSimulationNv::GetGridWidth() const {
        return mGridWidth;
    }

    /**
     * @brief 마지막으로 만든 격자의 Y 방향 입자 수를 반환합니다.
     */
    Uint32 FClothingSimulationNv::GetGridHeight() const {
        return mGridHeight;
    }
}
