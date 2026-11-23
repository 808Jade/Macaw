#include "PhysicsRuntime/FPhysicsScene.h"
#include "Core/Log/Log.h"

#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cmath>
#include <thread>

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

    namespace {
        /** 엔진은 Z-Up 좌표계이므로 중력은 -Z 방향입니다. Px와 엔진 타입 변환 헤더는 Transform 동기화 단계에서 만듭니다. */
        constexpr float GravityZ{-9.81f};

        /** 0 이하 설정값으로 나눗셈이 깨지지 않도록 하는 최소 시간입니다. */
        constexpr float MinStepDt{1.0e-4f};

        const char* ToString(EPhysicsTimeStepMode Mode) {
            switch (Mode) {
            case EPhysicsTimeStepMode::Variable:
                return "Variable";
            case EPhysicsTimeStepMode::Substep:
                return "Substep";
            case EPhysicsTimeStepMode::Fixed:
                return "Fixed";
            }

            return "Unknown";
        }
    }

    void FPhysicsScene::FRelease::operator()(physx::PxScene* Scene) const noexcept {
        Scene->release();
    }

    FPhysicsScene::FPhysicsScene(physx::PxPhysics& Physics, physx::PxCpuDispatcher& Dispatcher, const FPhysicsSettings& Settings)
        : mSettings{&Settings}, mLastMode{Settings.mMode} {
        physx::PxSceneDesc Desc{Physics.getTolerancesScale()};

        Desc.gravity = physx::PxVec3{0.0f, 0.0f, GravityZ};
        Desc.cpuDispatcher = &Dispatcher;
        Desc.filterShader = physx::PxDefaultSimulationFilterShader;

        if (!Desc.isValid()) {
            MACAW_LOG(LogPhysics, Error, "The PhysX scene description is invalid.");

            return;
        }

        mScene.reset(Physics.createScene(Desc));

        if (mScene == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX scene.");
        }
    }

    FPhysicsScene::~FPhysicsScene() = default;

    bool FPhysicsScene::IsValid() const {
        return mScene != nullptr;
    }

    FPhysicsStepPlan FPhysicsScene::Advance(float FrameDt) {
        PaceFrame();

        const FPhysicsStepPlan Plan{PlanSteps(FrameDt)};

        if (mScene != nullptr) {
            // simulate와 fetchResults는 1:1로 짝지어 호출해야 합니다. 스텝 시간은 항상 0보다 큽니다.
            for (Int32 Step{}; Step < Plan.mNumSteps; ++Step) {
                mScene->simulate(Plan.mStepDt);

                if (!mScene->fetchResults(true)) {
                    MACAW_LOG(LogPhysics, Error, "PxScene::fetchResults did not return results.");
                }
            }
        }

        RecordStats(FrameDt, Plan);

        return Plan;
    }

    FPhysicsStepPlan FPhysicsScene::PlanSteps(float FrameDt) {
        const FPhysicsSettings& Settings{*mSettings};
        FPhysicsStepPlan Plan{};

        if (Settings.mMode != mLastMode) {
            mAccumulator = 0.0f;
            mLastMode = Settings.mMode;
        }

        const bool ValidFrame{std::isfinite(FrameDt) && FrameDt > 0.0f};

        if (ValidFrame) {
            switch (Settings.mMode) {
            case EPhysicsTimeStepMode::Variable: {
                const float StepDt{std::min(FrameDt, std::max(Settings.mMaxFrameDt, MinStepDt))};

                Plan.mNumSteps = 1;
                Plan.mStepDt = StepDt;
                Plan.mDroppedTime = FrameDt - StepDt;
                break;
            }
            case EPhysicsTimeStepMode::Substep: {
                const float MaxDt{std::max(Settings.mMaxSubstepDt, MinStepDt)};
                const Int32 MaxSteps{std::max(Settings.mMaxSubsteps, 1)};
                const float Wanted{std::ceil(FrameDt / MaxDt)};

                if (Wanted > static_cast<float>(MaxSteps)) {
                    // 상한을 넘으면 스텝 크기를 MaxDt로 유지하고 모자라는 시간은 버립니다.
                    Plan.mNumSteps = MaxSteps;
                    Plan.mStepDt = MaxDt;
                    Plan.mDroppedTime = FrameDt - static_cast<float>(MaxSteps) * MaxDt;
                } else {
                    Plan.mNumSteps = std::max(static_cast<Int32>(Wanted), 1);
                    Plan.mStepDt = FrameDt / static_cast<float>(Plan.mNumSteps);
                }
                break;
            }
            case EPhysicsTimeStepMode::Fixed: {
                const float FixedDt{std::max(Settings.mFixedDt, MinStepDt)};
                const Int32 MaxSteps{std::max(Settings.mMaxFixedSteps, 1)};

                mAccumulator += FrameDt;

                const float Ready{std::floor(mAccumulator / FixedDt)};

                // 상한을 넘는 스텝은 버리고 한 스텝 미만의 나머지만 이월합니다.
                mAccumulator = std::max(mAccumulator - Ready * FixedDt, 0.0f);

                Plan.mNumSteps = static_cast<Int32>(std::min(Ready, static_cast<float>(MaxSteps)));
                Plan.mStepDt = FixedDt;
                Plan.mDroppedTime = (Ready - static_cast<float>(Plan.mNumSteps)) * FixedDt;
                break;
            }
            }
        }

        if (Settings.mMode == EPhysicsTimeStepMode::Fixed) {
            Plan.mAlpha = std::clamp(mAccumulator / std::max(Settings.mFixedDt, MinStepDt), 0.0f, 1.0f);
        }

        return Plan;
    }

    void FPhysicsScene::PaceFrame() {
        using Clock = std::chrono::steady_clock;

        const FPhysicsSettings& Settings{*mSettings};

        if (Settings.mTargetFrameMs > 0.0f && mHasLastAdvance) {
            float TargetMs{Settings.mTargetFrameMs};

            if (Settings.mJitterMs > 0.0f) {
                TargetMs += std::uniform_real_distribution<float>{-Settings.mJitterMs, Settings.mJitterMs}(mRandom);
            }

            const auto Period{std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float, std::milli>{std::max(TargetMs, 1.0f)})};
            const auto Deadline{mLastAdvance + Period};
            const auto SpinMargin{std::chrono::milliseconds{2}};

            // sleep_until은 수 ms의 오차가 있으므로 마지막 2ms는 양보하며 기다립니다.
            if (Clock::now() + SpinMargin < Deadline) {
                std::this_thread::sleep_until(Deadline - SpinMargin);
            }

            while (Clock::now() < Deadline) {
                std::this_thread::yield();
            }
        }

        mLastAdvance = Clock::now();
        mHasLastAdvance = true;
    }

    void FPhysicsScene::RecordStats(float FrameDt, const FPhysicsStepPlan& Plan) {
        if (!mSettings->mLogStepStats || !std::isfinite(FrameDt) || FrameDt <= 0.0f) {
            return;
        }

        mStatsWallTime += FrameDt;
        mStatsSimTime += static_cast<float>(Plan.mNumSteps) * Plan.mStepDt;
        mStatsDroppedTime += Plan.mDroppedTime;
        mStatsSteps += Plan.mNumSteps;
        ++mStatsFrames;

        if (mStatsWallTime < 1.0f) {
            return;
        }

        MACAW_LOG(LogPhysics, Log, "Step stats [{}]: frames={}, wall={:.3f}s, sim={:.3f}s, sim/wall={:.2f}, avgSteps={:.2f}, dropped={:.3f}s",
            ToString(mSettings->mMode), mStatsFrames, mStatsWallTime, mStatsSimTime, mStatsSimTime / mStatsWallTime,
            static_cast<float>(mStatsSteps) / static_cast<float>(mStatsFrames), mStatsDroppedTime);

        mStatsWallTime = 0.0f;
        mStatsSimTime = 0.0f;
        mStatsDroppedTime = 0.0f;
        mStatsFrames = 0;
        mStatsSteps = 0;
    }
}
