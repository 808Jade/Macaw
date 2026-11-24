/**
 * @file FVehicleManager.cpp
 * @brief 차량 입력을 적용하고 서스펜션 쿼리와 차량 업데이트를 실행합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FVehicleManager.h"
#include "Core/Literals/UnitLiterals.h"
#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"

#include <vehicle/PxVehicleDrive4W.h>
#include <vehicle/PxVehicleNoDrive.h>
#include <vehicle/PxVehicleUpdate.h>
#include <vehicle/PxVehicleUtilControl.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace PhysicsRuntime {

    namespace {
        using namespace Core::Literals;

        // PhysX의 클러치 강도는 kg·m²/s로 전달합니다.
        using FClutchStrength = Core::TQuantity<Core::Dim::MomentOfInertia - Core::Dim::Time, float>;

        constexpr physx::PxU32 DrivableSurfaceFlag{1u << 31};
        constexpr physx::PxU32 StandardTireType{0};

        physx::PxQueryHitType::Enum FilterSuspensionRay(physx::PxFilterData, physx::PxFilterData ShapeFilter, const void*, physx::PxU32, physx::PxHitFlags&) {
            return (ShapeFilter.word3 & DrivableSurfaceFlag) != 0 ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
        }

        struct FFreeVehicle {
            void operator()(physx::PxVehicleWheels* Vehicle) const {
                switch (Vehicle->getVehicleType()) {
                case physx::PxVehicleTypes::eDRIVE4W:
                    static_cast<physx::PxVehicleDrive4W*>(Vehicle)->free();
                    break;
                default:
                    static_cast<physx::PxVehicleNoDrive*>(Vehicle)->free();
                    break;
                }
            }
        };

        /** @brief 엔진이 있는 차량이면 공통 구동 상태를 반환합니다. NoDrive는 nullptr입니다. */
        physx::PxVehicleDrive* AsDrive(physx::PxVehicleWheels& Vehicle) {
            return Vehicle.getVehicleType() != physx::PxVehicleTypes::eNODRIVE ? static_cast<physx::PxVehicleDrive*>(&Vehicle) : nullptr;
        }

        const physx::PxVehicleDrive* AsDrive(const physx::PxVehicleWheels& Vehicle) {
            return AsDrive(const_cast<physx::PxVehicleWheels&>(Vehicle));
        }

        void SetDriveToRestState(physx::PxVehicleDrive& Drive) {
            switch (Drive.getVehicleType()) {
            case physx::PxVehicleTypes::eDRIVE4W:
                static_cast<physx::PxVehicleDrive4W&>(Drive).setToRestState();
                break;
            default:
                break;
            }
        }

        struct FDriveInput {
            float mThrottle{};
            float mBrake{};
            float mSteering{};
        };

        struct FPedals {
            float mAccel{};
            float mBrake{};
        };

        /** @brief 엔진·기어·자동 변속·클러치를 설정합니다. 차종별 차이는 최대 토크로 지정합니다. */
        void FillDrivetrain(physx::PxVehicleDriveSimData& Data, Core::FTorque PeakTorque) {
            physx::PxVehicleEngineData Engine{};
            Engine.mPeakTorque = PeakTorque.GetBase();
            Engine.mMaxOmega = (540_radps).GetBase();
            Engine.mTorqueCurve.clear();
            Engine.mTorqueCurve.addPair(0.0f, 0.82f);
            Engine.mTorqueCurve.addPair(0.35f, 1.0f);
            Engine.mTorqueCurve.addPair(0.72f, 0.86f);
            Engine.mTorqueCurve.addPair(1.0f, 0.45f);

            physx::PxVehicleGearsData Gears{};
            Gears.mRatios[physx::PxVehicleGearsData::eREVERSE] = -3.4f;
            Gears.mRatios[physx::PxVehicleGearsData::eFIRST] = 3.6f;
            Gears.mRatios[physx::PxVehicleGearsData::eSECOND] = 2.3f;
            Gears.mSwitchTime = (0.24_s).GetBase();

            physx::PxVehicleAutoBoxData AutoBox{};
            AutoBox.setLatency((0.65_s).GetBase());
            for (auto Gear{static_cast<physx::PxU32>(physx::PxVehicleGearsData::eFIRST)}; Gear < Gears.mNbRatios; ++Gear) {
                AutoBox.mUpRatios[Gear] = 0.58f;
                AutoBox.mDownRatios[Gear] = 0.27f;
            }

            physx::PxVehicleClutchData Clutch{};
            Clutch.mStrength = FClutchStrength::FromBase(10.0f).GetBase();

            Data.setEngineData(Engine);
            Data.setGearsData(Gears);
            Data.setAutoBoxData(AutoBox);
            Data.setClutchData(Clutch);
        }

        void StartInFirstGear(physx::PxVehicleDrive& Drive) {
            Drive.mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eFIRST);
            Drive.mDriveDynData.setUseAutoGears(true);
        }

        /** @brief 진행 방향을 바꿀 때는 먼저 멈추고, 전진 기어에 돌아오면 자동 변속을 이어갑니다. */
        FPedals ResolveDriveDirection(physx::PxVehicleDrive& Drive, const FDriveInput& Input) {
            auto& Dynamics{Drive.mDriveDynData};
            FPedals Pedals{std::abs(Input.mThrottle), Input.mBrake};

            if (Pedals.mAccel > 0.0f && Pedals.mBrake == 0.0f) {
                const bool Reverse{Input.mThrottle < 0.0f};
                const bool InReverse{Dynamics.getCurrentGear() == physx::PxVehicleGearsData::eREVERSE};
                if (Reverse != InReverse) {
                    constexpr auto DirectionChangeSpeed{0.5_mps};
                    const auto ForwardSpeed{Core::FSpeed::FromBase(Drive.computeForwardSpeed())};
                    if (Core::Math::Abs(ForwardSpeed) > DirectionChangeSpeed) {
                        Pedals.mBrake = std::max(Pedals.mBrake, Pedals.mAccel);
                        Pedals.mAccel = 0.0f;
                    } else {
                        Dynamics.forceGearChange(Reverse ? physx::PxVehicleGearsData::eREVERSE : physx::PxVehicleGearsData::eFIRST);
                        Dynamics.setUseAutoGears(!Reverse);
                    }
                }
            }

            return Pedals;
        }

        void ResetWheelResults(std::span<physx::PxWheelQueryResult> Results, const physx::PxVehicleWheelsSimData& Wheels,
                               const physx::PxRigidDynamic& Actor) {
            // 첫 물리 스텝 전에는 차체 로컬 기준 자세를 사용합니다.
            const auto CenterOfMass{Actor.getCMassLocalPose()};
            for (physx::PxU32 Wheel{}; Wheel < Results.size(); ++Wheel) {
                auto& Result{Results[Wheel]};
                Result = {};
                Result.localPose.p = CenterOfMass.transform(Wheels.getWheelCentreOffset(Wheel));
            }
        }

        void ApplyDrive4WInput(physx::PxVehicleDrive4W& Drive, const FDriveInput& Input, Core::FTime StepTime, bool InAir) {
            constexpr auto AccelRise{4.0f / 1_s};
            constexpr auto AccelFall{8.0f / 1_s};
            constexpr auto BrakeRise{8.0f / 1_s};
            constexpr auto BrakeFall{10.0f / 1_s};
            constexpr auto SteerRise{3.0f / 1_s};
            constexpr auto SteerFall{5.0f / 1_s};
            static constexpr physx::PxVehiclePadSmoothingData Smoothing{
                {AccelRise.GetBase(), BrakeRise.GetBase(), BrakeRise.GetBase(), SteerRise.GetBase(), SteerRise.GetBase()},
                {AccelFall.GetBase(), BrakeFall.GetBase(), BrakeFall.GetBase(), SteerFall.GetBase(), SteerFall.GetBase()}
            };
            static const auto SteerBySpeed{[] {
                physx::PxFixedSizeLookupTable<8> Table{};
                Table.addPair((0_mps).GetBase(), 1.0f);
                Table.addPair((7_mps).GetBase(), 0.70f);
                Table.addPair((15_mps).GetBase(), 0.40f);
                Table.addPair((25_mps).GetBase(), 0.22f);
                Table.addPair((36_mps).GetBase(), 0.12f);
                return Table;
            }()};

            const auto Pedals{ResolveDriveDirection(Drive, Input)};

            physx::PxVehicleDrive4WRawInputData Raw{};
            Raw.setAnalogAccel(Pedals.mBrake > 0.0f ? 0.0f : Pedals.mAccel);
            Raw.setAnalogBrake(Pedals.mBrake);
            Raw.setAnalogSteer(Input.mSteering);
            physx::PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(Smoothing, SteerBySpeed, Raw, StepTime.GetBase(), InAir, Drive);
        }
    }

    struct FVehicleManager::FWheelControl {
        Core::FTorque mDriveTorque{};
        Core::FTorque mBrakeTorque{};
        Core::FAngle mSteerAngle{};
    };

    struct FVehicleManager::FVehicle {
        std::unique_ptr<physx::PxVehicleWheels, FFreeVehicle> mDrive{};
        Core::TArray<FWheelControl> mWheelControls{};
        Core::TArray<physx::PxWheelQueryResult> mWheelResults{};
        FDriveInput mDriveInput{};
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

    bool FVehicleManager::PrepareVehicle(const FBodyInstance& Body, const physx::PxVehicleWheelsSimData& Wheels, FVehicle& Vehicle) {
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
        for (const FVehicle& Entry : mVehicles) {
            TotalWheels += Entry.mWheelResults.size();
        }

        if (TotalWheels > std::numeric_limits<Uint32>::max() || !EnsureRaycastCapacity(static_cast<Uint32>(TotalWheels))) {
            return false;
        }

        Vehicle.mWheelResults.resize(WheelCount);
        ResetWheelResults(Vehicle.mWheelResults, Wheels, *Actor);

        return true;
    }

    bool FVehicleManager::RegisterNoDrive(const FBodyInstance& Body, const physx::PxVehicleWheelsSimData& Wheels) {
        FVehicle Vehicle{};
        if (!PrepareVehicle(Body, Wheels, Vehicle)) {
            return false;
        }

        auto* Actor{Body.GetPxRigidDynamic()};
        Vehicle.mWheelControls.resize(Wheels.getNbWheels());
        Vehicle.mDrive.reset(physx::PxVehicleNoDrive::create(mRuntime.GetPhysics(), Actor, Wheels));
        if (Vehicle.mDrive == nullptr || Vehicle.mDrive->getRigidDynamicActor() != Actor) {
            return false;
        }

        mVehicles.push_back(std::move(Vehicle));

        return true;
    }

    bool FVehicleManager::RegisterDrive4W(const FBodyInstance& Body, const physx::PxVehicleWheelsSimData& Wheels) {
        using physx::PxVehicleDrive4WWheelOrder;
        constexpr physx::PxU32 WheelCount{4};
        FVehicle Vehicle{};
        if (Wheels.getNbWheels() != WheelCount || !PrepareVehicle(Body, Wheels, Vehicle)) {
            return false;
        }

        const auto& FrontLeft{Vehicle.mWheelResults[PxVehicleDrive4WWheelOrder::eFRONT_LEFT].localPose.p};
        const auto& FrontRight{Vehicle.mWheelResults[PxVehicleDrive4WWheelOrder::eFRONT_RIGHT].localPose.p};
        const auto& RearLeft{Vehicle.mWheelResults[PxVehicleDrive4WWheelOrder::eREAR_LEFT].localPose.p};
        const auto& RearRight{Vehicle.mWheelResults[PxVehicleDrive4WWheelOrder::eREAR_RIGHT].localPose.p};
        if (!(FrontLeft.x > RearLeft.x && FrontRight.x > RearRight.x && FrontRight.y > FrontLeft.y && RearRight.y > RearLeft.y)) {
            return false;
        }

        const auto LeftWheelbase{Core::FLength::FromBase(FrontLeft.x - RearLeft.x)};
        const auto RightWheelbase{Core::FLength::FromBase(FrontRight.x - RearRight.x)};
        const auto Wheelbase{LeftWheelbase * 0.5f + RightWheelbase * 0.5f};
        const auto FrontWidth{Core::FLength::FromBase(FrontRight.y - FrontLeft.y)};
        const auto RearWidth{Core::FLength::FromBase(RearRight.y - RearLeft.y)};
        if (!std::isfinite(Wheelbase.GetBase()) || !std::isfinite(FrontWidth.GetBase()) || !std::isfinite(RearWidth.GetBase())) {
            return false;
        }

        physx::PxVehicleAckermannGeometryData Geometry{};
        Geometry.mAccuracy = 1.0f;
        Geometry.mAxleSeparation = Wheelbase.GetBase();
        Geometry.mFrontWidth = FrontWidth.GetBase();
        Geometry.mRearWidth = RearWidth.GetBase();

        auto FreeWheels{[](physx::PxVehicleWheelsSimData* Data) { Data->free(); }};
        std::unique_ptr<physx::PxVehicleWheelsSimData, decltype(FreeWheels)> DrivenWheels{physx::PxVehicleWheelsSimData::allocate(WheelCount)};
        if (DrivenWheels == nullptr) {
            return false;
        }

        *DrivenWheels = Wheels;
        constexpr auto MaxSteerAngle{32_deg};
        for (const auto Wheel : {PxVehicleDrive4WWheelOrder::eFRONT_LEFT, PxVehicleDrive4WWheelOrder::eFRONT_RIGHT}) {
            auto Data{DrivenWheels->getWheelData(Wheel)};
            Data.mMaxSteer = MaxSteerAngle.GetBase();
            DrivenWheels->setWheelData(Wheel, Data);
        }

        physx::PxVehicleDifferential4WData Differential{};
        Differential.mType = physx::PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;

        physx::PxVehicleDriveSimData4W DriveData{};
        FillDrivetrain(DriveData, 420_Nm);
        DriveData.setDiffData(Differential);
        DriveData.setAckermannGeometryData(Geometry);

        auto* Actor{Body.GetPxRigidDynamic()};
        auto* Drive{physx::PxVehicleDrive4W::create(mRuntime.GetPhysics(), Actor, *DrivenWheels, DriveData, 0)};
        Vehicle.mDrive.reset(Drive);
        if (Drive == nullptr || Drive->getRigidDynamicActor() != Actor) {
            return false;
        }

        StartInFirstGear(*Drive);
        mVehicles.push_back(std::move(Vehicle));

        return true;
    }

    bool FVehicleManager::SetDriveInput(const FBodyInstance& Body, float Throttle, float Brake, float Steering) {
        auto* Vehicle{FindVehicle(Body)};
        if (Vehicle == nullptr || AsDrive(*Vehicle->mDrive) == nullptr ||
            !std::isfinite(Throttle) || !std::isfinite(Brake) || !std::isfinite(Steering)) {
            return false;
        }

        Vehicle->mDriveInput = {std::clamp(Throttle, -1.0f, 1.0f), std::clamp(Brake, 0.0f, 1.0f), std::clamp(Steering, -1.0f, 1.0f)};
        return true;
    }

    bool FVehicleManager::ResetDrive(const FBodyInstance& Body) {
        auto* Vehicle{FindVehicle(Body)};
        auto* Drive{Vehicle != nullptr ? AsDrive(*Vehicle->mDrive) : nullptr};
        if (Drive == nullptr) {
            return false;
        }

        SetDriveToRestState(*Drive);
        StartInFirstGear(*Drive);
        Vehicle->mDriveInput = {};
        ResetWheelResults(Vehicle->mWheelResults, Drive->mWheelsSimData, *Drive->getRigidDynamicActor());
        return true;
    }

    Int32 FVehicleManager::GetDriveGear(const FBodyInstance& Body) const {
        const auto* Vehicle{FindVehicle(Body)};
        const auto* Drive{Vehicle != nullptr ? AsDrive(*Vehicle->mDrive) : nullptr};
        if (Drive == nullptr) {
            return 0;
        }

        return static_cast<Int32>(Drive->mDriveDynData.getCurrentGear()) - physx::PxVehicleGearsData::eNEUTRAL;
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

    FVehicleManager::FVehicle* FVehicleManager::FindVehicle(const FBodyInstance& Body) {
        return const_cast<FVehicle*>(std::as_const(*this).FindVehicle(Body));
    }

    const FVehicleManager::FVehicle* FVehicleManager::FindVehicle(const FBodyInstance& Body) const {
        const auto* Actor{Body.GetPxRigidDynamic()};
        const auto Vehicle{std::ranges::find_if(mVehicles, [Actor](const FVehicle& Entry) {
            return Entry.mDrive->getRigidDynamicActor() == Actor;
        })};

        return Vehicle != mVehicles.end() ? &*Vehicle : nullptr;
    }

    FVehicleManager::FWheelControl* FVehicleManager::FindWheelControl(const FBodyInstance& Body, Uint32 WheelIndex) {
        auto* Vehicle{FindVehicle(Body)};
        return Vehicle != nullptr && WheelIndex < Vehicle->mWheelControls.size() ? &Vehicle->mWheelControls[WheelIndex] : nullptr;
    }

    bool FVehicleManager::SetDriveTorque(const FBodyInstance& Body, Uint32 WheelIndex, Core::FTorque Torque) {
        auto* Control{FindWheelControl(Body, WheelIndex)};

        if (Control == nullptr || !std::isfinite(Torque.GetBase())) {
            return false;
        }

        Control->mDriveTorque = Torque;

        return true;
    }

    bool FVehicleManager::SetBrakeTorque(const FBodyInstance& Body, Uint32 WheelIndex, Core::FTorque Torque) {
        auto* Control{FindWheelControl(Body, WheelIndex)};

        if (Control == nullptr || !std::isfinite(Torque.GetBase()) || Torque < 0_Nm) {
            return false;
        }

        Control->mBrakeTorque = Torque;

        return true;
    }

    bool FVehicleManager::SetSteerAngle(const FBodyInstance& Body, Uint32 WheelIndex, Core::FAngle Angle) {
        auto* Control{FindWheelControl(Body, WheelIndex)};

        if (Control == nullptr || !std::isfinite(Angle.GetBase())) {
            return false;
        }

        Control->mSteerAngle = Angle;

        return true;
    }

    std::span<const physx::PxWheelQueryResult> FVehicleManager::GetWheelQueryResults(const FBodyInstance& Body) const {
        const auto* Vehicle{FindVehicle(Body)};
        return Vehicle != nullptr ? std::span<const physx::PxWheelQueryResult>{Vehicle->mWheelResults} : std::span<const physx::PxWheelQueryResult>{};
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

    void FVehicleManager::Step(Core::FTime StepTime) {
        if (mVehicles.empty() || mScene.getGravity().isZero()) {
            return;
        }

        FRaycastBatch& Batch{*mRaycastBatch};
        for (FVehicle& Vehicle : mVehicles) {
            const physx::PxVehicleWheelQueryResult Results{Vehicle.mWheelResults.data(), static_cast<physx::PxU32>(Vehicle.mWheelResults.size())};
            switch (Vehicle.mDrive->getVehicleType()) {
            case physx::PxVehicleTypes::eDRIVE4W: {
                auto& Drive{static_cast<physx::PxVehicleDrive4W&>(*Vehicle.mDrive)};
                ApplyDrive4WInput(Drive, Vehicle.mDriveInput, StepTime, physx::PxVehicleIsInAir(Results));
                break;
            }
            default: {
                auto& Drive{static_cast<physx::PxVehicleNoDrive&>(*Vehicle.mDrive)};
                for (physx::PxU32 Wheel{}; Wheel < Results.nbWheelQueryResults; ++Wheel) {
                    const auto& Control{Vehicle.mWheelControls[Wheel]};
                    Drive.setDriveTorque(Wheel, Control.mDriveTorque.GetBase());
                    Drive.setBrakeTorque(Wheel, Control.mBrakeTorque.GetBase());
                    Drive.setSteerAngle(Wheel, Control.mSteerAngle.GetBase());
                }
                break;
            }
            }

            Batch.mStepVehicles.push_back(Vehicle.mDrive.get());
            Batch.mStepWheelResults.push_back(Results);
        }

        const auto VehicleCount{static_cast<physx::PxU32>(Batch.mStepVehicles.size())};
        const auto RaycastCount{static_cast<physx::PxU32>(Batch.mRaycastResults.size())};
        physx::PxVehicleSuspensionRaycasts(Batch.mQuery.get(), VehicleCount, Batch.mStepVehicles.data(), RaycastCount, Batch.mRaycastResults.data());
        physx::PxVehicleUpdates(StepTime.GetBase(), mScene.getGravity(), *Batch.mFriction, VehicleCount, Batch.mStepVehicles.data(), Batch.mStepWheelResults.data());

        Batch.mStepVehicles.clear();
        Batch.mStepWheelResults.clear();
    }

}
