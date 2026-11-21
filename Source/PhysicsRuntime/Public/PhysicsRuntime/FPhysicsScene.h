#pragma once

#include "Core/Math/FVector.h"
#include <PxPhysicsAPI.h>

namespace PhysicsRuntime {
    class FPhysicsRuntime;

    // 월드 하나의 PxScene을 소유하고 한 스텝씩 진행합니다.
    class FPhysicsScene {
    public:
        FPhysicsScene();
        ~FPhysicsScene();

        FPhysicsScene(const FPhysicsScene&) = delete;
        FPhysicsScene& operator=(const FPhysicsScene&) = delete;

    public:
        bool InitPhysScene(FPhysicsRuntime& Runtime);
        void TermPhysScene();

        // 이번 프레임의 중력과 시간을 받습니다.
        void SetUpForFrame(const FVector& Gravity, float DeltaSeconds);

        // 시뮬레이션을 시작합니다.
        void StartFrame();

        // 시뮬레이션이 끝날 때까지 기다려 결과를 확정합니다.
        void EndFrame();

        physx::PxScene* GetPxScene() const;

    private:
        physx::PxScene* mPhysXScene{};

        float mDeltaSeconds{};

        // 한 스텝의 최대 시간
        float mMaxPhysicsDeltaTime{1.0f / 30.0f};

        // simulate와 fetchResults 사이인지
        bool mPhysXSceneExecuting{};
    };
}
