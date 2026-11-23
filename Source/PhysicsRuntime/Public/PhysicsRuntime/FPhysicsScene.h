#pragma once

#include "Core/Types.h"
#include "PhysicsRuntime/FPhysicsSettings.h"

#include <chrono>
#include <memory>
#include <random>

namespace physx {
    class PxCpuDispatcher;
    class PxPhysics;
    class PxScene;
}

namespace PhysicsRuntime {
    /**
     * @brief 한 프레임의 시간이 몇 번, 어떤 크기의 스텝으로 변환되었는지 나타냅니다.
     */
    struct FPhysicsStepPlan {
        /** 이번 프레임에 실행할 스텝 수입니다. 0일 수 있습니다. */
        Int32 mNumSteps{};

        /** 스텝 1회의 시간입니다. */
        float mStepDt{};

        /** 마지막 두 물리 상태 사이의 보간 비율입니다. Fixed 모드에서만 1 미만이 될 수 있습니다. */
        float mAlpha{1.0f};

        /** 상한 때문에 물리에 반영하지 못하고 버린 시간입니다. */
        float mDroppedTime{};
    };

    /**
     * @brief 월드 하나가 가지는 PhysX 씬과 시간 진행을 관리합니다.
     * @details PxScene이 내부에 CPU 디스패처 포인터를 보관하므로 디스패처와 설정은 이 객체보다 오래 살아야 합니다.
     */
    class FPhysicsScene {
    private:
        struct FRelease {
            void operator()(physx::PxScene* Scene) const noexcept;
        };

    public:
        FPhysicsScene(physx::PxPhysics& Physics, physx::PxCpuDispatcher& Dispatcher, const FPhysicsSettings& Settings);
        ~FPhysicsScene();

        FPhysicsScene(const FPhysicsScene&) = delete;
        FPhysicsScene& operator=(const FPhysicsScene&) = delete;

    public:
        /**
         * @brief PxScene 생성에 성공했는지 반환합니다.
         */
        bool IsValid() const;

        /**
         * @brief 프레임 시간을 스텝으로 변환해 simulate와 fetchResults를 짝으로 실행합니다.
         * @param[in] FrameDt 이전 프레임 이후 경과한 실제 시간이며 단위는 초입니다.
         * @return 실행한 스텝 정보입니다.
         */
        FPhysicsStepPlan Advance(float FrameDt);

    private:
        FPhysicsStepPlan PlanSteps(float FrameDt);
        void PaceFrame();
        void RecordStats(float FrameDt, const FPhysicsStepPlan& Plan);

    private:
        std::unique_ptr<physx::PxScene, FRelease> mScene{};
        const FPhysicsSettings* mSettings{};

        /** Fixed 모드에서 아직 스텝으로 바꾸지 못한 실제 시간입니다. */
        float mAccumulator{};
        EPhysicsTimeStepMode mLastMode{};

        std::chrono::steady_clock::time_point mLastAdvance{};
        bool mHasLastAdvance{};
        std::mt19937 mRandom{1u};

        float mStatsWallTime{};
        float mStatsSimTime{};
        float mStatsDroppedTime{};
        Int32 mStatsFrames{};
        Int32 mStatsSteps{};
    };
}
