/**
 * @file FVehicleManager.cpp
 * @brief 차량 입력을 적용하고 서스펜션 쿼리와 차량 업데이트를 실행합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FVehicleManager.h"
#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"

#include <vehicle/PxVehicleNoDrive.h>
#include <vehicle/PxVehicleUpdate.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace PhysicsRuntime {

    namespace {
        constexpr physx::PxU32 DrivableSurfaceFlag{1u << 31};
        constexpr physx::PxU32 StandardTireType{0};

        physx::PxQueryHitType::Enum FilterSuspensionRay(physx::PxFilterData, physx::PxFilterData ShapeFilter, const void*, physx::PxU32, physx::PxHitFlags&) {
            return (ShapeFilter.word3 & DrivableSurfaceFlag) != 0 ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
        }

        struct FFreeNoDrive {
            void operator()(physx::PxVehicleNoDrive* Vehicle) const {
                Vehicle->free();
            }
        };
    }

    struct FVehicleManager::FVehicle {
        std::unique_ptr<physx::PxVehicleNoDrive, FFreeNoDrive> mDrive{};
        Core::TArray<float> mDriveTorques{};
        Core::TArray<physx::PxWheelQueryResult> mWheelResults{};
    };

    struct FVehicleManager::FRaycastBatch {
        Core::TArray<physx::PxRaycastQueryResult> mRaycastResults{};
        Core::TArray<physx::PxRaycastHit> mRaycastHits{};
        Core::TArray<physx::PxVehicleWheels*> mStepVehicles{};
        Core::TArray<physx::PxVehicleWheelQueryResult> mStepWheelResults{};
        std::unique_ptr<physx::PxVehicleDrivableSurfaceToTireFrictionPairs, FPhysXRelease> mFriction{};
        std::unique_ptr<physx::PxBatchQuery, FPhysXRelease> mQuery{};
    };

    FVehicleManager::FVehicleManager(physx::PxScene& Scene, FPhysicsRuntime& Runtime)
        : mScene{Scene}, mRuntime{Runtime} {
    }

    FVehicleManager::~FVehicleManager() = default;

    bool FVehicleManager::RegisterNoDrive(const FBodyInstance& Body, const physx::PxVehicleWheelsSimData& Wheels) {
        auto* Actor{Body.GetPxRigidDynamic()};
        const physx::PxU32 WheelCount{Wheels.getNbWheels()};

        if (Actor == nullptr || Actor->getScene() != &mScene || Actor->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC) ||
            WheelCount == 0 || WheelCount > PX_MAX_NB_WHEELS || !GetWheelQueryResults(Body).empty()) {
            return false;
        }

        const auto ShapeCount{static_cast<physx::PxI32>(Actor->getNbShapes())};
        for (physx::PxU32 Wheel{}; Wheel < WheelCount; ++Wheel) {
            if (Wheels.getWheelShapeMapping(Wheel) >= ShapeCount || Wheels.getTireData(Wheel).mType != StandardTireType) {
                return false;
            }
        }

        Index_t TotalWheels{WheelCount};
        for (const FVehicle& Vehicle : mVehicles) {
            TotalWheels += Vehicle.mWheelResults.size();
        }

        if (TotalWheels > std::numeric_limits<Uint32>::max() || !EnsureRaycastCapacity(static_cast<Uint32>(TotalWheels))) {
            return false;
        }

        FVehicle Vehicle{};
        Vehicle.mDriveTorques.resize(WheelCount);
        Vehicle.mWheelResults.resize(WheelCount);
        Vehicle.mDrive.reset(physx::PxVehicleNoDrive::create(mRuntime.GetPhysics(), Actor, Wheels));

        if (Vehicle.mDrive == nullptr || Vehicle.mDrive->getRigidDynamicActor() != Actor) {
            return false;
        }

        mVehicles.push_back(std::move(Vehicle));

        return true;
    }

    bool FVehicleManager::UnregisterVehicle(const FBodyInstance& Body) {
        const auto* Actor{Body.GetPxRigidDynamic()};

        if (Actor == nullptr) {
            return false;
        }

        return std::erase_if(mVehicles, [Actor](const FVehicle& Vehicle) {
            return Vehicle.mDrive->getRigidDynamicActor() == Actor;
        }) != 0;
    }

    bool FVehicleManager::SetDriveTorque(const FBodyInstance& Body, Uint32 WheelIndex, float Torque) {
        const auto* Actor{Body.GetPxRigidDynamic()};
        const auto Vehicle{std::ranges::find_if(mVehicles, [Actor](const FVehicle& Entry) {
            return Entry.mDrive->getRigidDynamicActor() == Actor;
        })};

        if (Vehicle == mVehicles.end() || WheelIndex >= Vehicle->mDriveTorques.size() || !std::isfinite(Torque)) {
            return false;
        }

        Vehicle->mDriveTorques[WheelIndex] = Torque;

        return true;
    }

    std::span<const physx::PxWheelQueryResult> FVehicleManager::GetWheelQueryResults(const FBodyInstance& Body) const {
        const auto* Actor{Body.GetPxRigidDynamic()};
        const auto Vehicle{std::ranges::find_if(mVehicles, [Actor](const FVehicle& Entry) {
            return Entry.mDrive->getRigidDynamicActor() == Actor;
        })};

        return Vehicle != mVehicles.end() ? std::span<const physx::PxWheelQueryResult>{Vehicle->mWheelResults} : std::span<const physx::PxWheelQueryResult>{};
    }

    Index_t FVehicleManager::GetVehicleCount() const {
        return mVehicles.size();
    }

    void FVehicleManager::SetDrivableSurface(physx::PxShape& Shape, bool Drivable) {
        auto Filter{Shape.getQueryFilterData()};
        Filter.word3 = Drivable ? (Filter.word3 | DrivableSurfaceFlag) : (Filter.word3 & ~DrivableSurfaceFlag);
        Shape.setQueryFilterData(Filter);
    }

    bool FVehicleManager::EnsureRaycastCapacity(Uint32 WheelCount) {
        if (mRaycastBatch != nullptr && mRaycastBatch->mRaycastResults.size() >= WheelCount) {
            return true;
        }

        auto Batch{std::make_unique<FRaycastBatch>()};
        Batch->mRaycastResults.resize(WheelCount);
        Batch->mRaycastHits.resize(WheelCount);
        Batch->mFriction.reset(physx::PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1));

        if (Batch->mFriction == nullptr) {
            return false;
        }

        const physx::PxMaterial* Materials[]{mRuntime.GetDefaultMaterial()};
        const physx::PxVehicleDrivableSurfaceType Surfaces[]{{0}};
        Batch->mFriction->setup(1, 1, Materials, Surfaces);

        physx::PxBatchQueryDesc Desc{WheelCount, 0, 0};
        Desc.preFilterShader = FilterSuspensionRay;
        Desc.queryMemory.userRaycastResultBuffer = Batch->mRaycastResults.data();
        Desc.queryMemory.userRaycastTouchBuffer = Batch->mRaycastHits.data();
        Desc.queryMemory.raycastTouchBufferSize = WheelCount;
        Batch->mQuery.reset(mScene.createBatchQuery(Desc));

        if (Batch->mQuery == nullptr) {
            return false;
        }

        mRaycastBatch = std::move(Batch);

        return true;
    }

    void FVehicleManager::Step(float StepSeconds) {
        if (mVehicles.empty() || mScene.getGravity().isZero()) {
            return;
        }

        FRaycastBatch& Batch{*mRaycastBatch};
        for (FVehicle& Vehicle : mVehicles) {
            const auto WheelCount{static_cast<physx::PxU32>(Vehicle.mDriveTorques.size())};

            for (physx::PxU32 Wheel{}; Wheel < WheelCount; ++Wheel) {
                Vehicle.mDrive->setDriveTorque(Wheel, Vehicle.mDriveTorques[Wheel]);
            }

            Batch.mStepVehicles.push_back(Vehicle.mDrive.get());
            Batch.mStepWheelResults.push_back({Vehicle.mWheelResults.data(), WheelCount});
        }

        const auto VehicleCount{static_cast<physx::PxU32>(Batch.mStepVehicles.size())};
        const auto RaycastCount{static_cast<physx::PxU32>(Batch.mRaycastResults.size())};
        physx::PxVehicleSuspensionRaycasts(Batch.mQuery.get(), VehicleCount, Batch.mStepVehicles.data(), RaycastCount, Batch.mRaycastResults.data());
        physx::PxVehicleUpdates(StepSeconds, mScene.getGravity(), *Batch.mFriction, VehicleCount, Batch.mStepVehicles.data(), Batch.mStepWheelResults.data());

        Batch.mStepVehicles.clear();
        Batch.mStepWheelResults.clear();
    }

}
