/**
 * @file FClothingSimulationNv.cpp
 * @brief NvCloth 솔버로 천 입자를 시뮬레이션하는 클래스를 구현합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FClothingSimulationNv.h"
#include "PhysicsRuntime/PhysicsConversion.h"
#include "PhysicsRuntime/PhysicsStats.h"
#include "Core/Log/Log.h"

#include <NvCloth/Cloth.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Factory.h>
#include <NvCloth/PhaseConfig.h>
#include <NvCloth/Solver.h>
#include <NvClothExt/ClothFabricCooker.h>
#include <NvClothExt/ClothMeshDesc.h>

#include <algorithm>

namespace PhysicsRuntime {

    /**
     * @brief 천 시뮬레이션 로그 범주와 상수입니다.
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
     * @brief 팩토리로 솔버를 만듭니다.
     */
    bool FClothingSimulationNv::Initialize(nv::cloth::Factory* Factory) {
        if (mSolver != nullptr) {
            return true;
        }

        if (Factory == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot initialize cloth simulation without a factory.");

            return false;
        }

        mFactory = Factory;
        mSolver = mFactory->createSolver();

        if (mSolver == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot create the NvCloth solver.");
            mFactory = nullptr;

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

        Core::TArray<physx::PxVec4> Particles{};
        Particles.reserve(NumParticles);

        for (Uint32 Index{}; Index < NumParticles; ++Index) {
            Particles.emplace_back(Points[Index], InvMasses[Index]);
        }

        nv::cloth::Cloth* Cloth{mFactory->createCloth(nv::cloth::Range<const physx::PxVec4>{Particles.data(), Particles.data() + Particles.size()}, *Fabric)};

        if (Cloth == nullptr) {
            MACAW_LOG(LogCloth, Error, "Cannot create cloth for the {}x{} grid.", Width, Height);
            Fabric->decRefCount();

            return false;
        }

        // 위상마다 설정을 넣어야 솔버가 그 위상의 거리 제약을 풉니다.
        Core::TArray<nv::cloth::PhaseConfig> PhaseConfigs(Fabric->getNumPhases());

        for (Index_t PhaseIndex{}; PhaseIndex < PhaseConfigs.size(); ++PhaseIndex) {
            PhaseConfigs[PhaseIndex].mPhaseIndex = static_cast<uint16_t>(PhaseIndex);
        }

        Cloth->setPhaseConfig(nv::cloth::Range<const nv::cloth::PhaseConfig>{PhaseConfigs.data(), PhaseConfigs.data() + PhaseConfigs.size()});
        Cloth->setDamping(physx::PxVec3{GridDamping, GridDamping, GridDamping});

        mSolver->addCloth(Cloth);
        mActors.push_back({Fabric, Cloth});
        mGridWidth = Width;
        mGridHeight = Height;

        MACAW_LOG(LogCloth, Log, "Created a {}x{} grid cloth with {} phases.", Width, Height, PhaseConfigs.size());

        return true;
    }

    /**
     * @brief 등록한 천을 솔버에서 빼고 천과 패브릭을 해제합니다.
     */
    void FClothingSimulationNv::DestroyActors() {
        for (FClothingActorNv& Actor : mActors) {
            if (mSolver != nullptr) {
                mSolver->removeCloth(Actor.mCloth);
            }

            delete Actor.mCloth;
            Actor.mFabric->decRefCount();
        }

        mActors.clear();
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
            Actor.mCloth->setGravity(ToPxVec3(mGravity));
            NumParticles += Actor.mCloth->getNumParticles();
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
     * @brief 천 하나의 현재 입자 위치를 복사합니다.
     */
    bool FClothingSimulationNv::GetParticles(Index_t ActorIndex, Core::TArray<FVector>& OutPositions) const {
        OutPositions.clear();

        if (ActorIndex >= mActors.size()) {
            return false;
        }

        const nv::cloth::Cloth& Cloth{*mActors[ActorIndex].mCloth};
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
