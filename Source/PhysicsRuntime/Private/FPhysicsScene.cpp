#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"
#include "Core/Log/Log.h"

#include <algorithm>

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

    FPhysicsScene::FPhysicsScene() = default;

    FPhysicsScene::~FPhysicsScene() {
        TermPhysScene();
    }

    bool FPhysicsScene::InitPhysScene(FPhysicsRuntime& Runtime) {
        if (mPhysXScene != nullptr) {
            return true;
        }

        physx::PxPhysics* Physics{Runtime.GetPhysics()};
        physx::PxCpuDispatcher* Dispatcher{Runtime.GetDispatcher()};

        if (Physics == nullptr || Dispatcher == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a physics scene before the PhysX runtime is initialized.");

            return false;
        }

        physx::PxSceneDesc SceneDesc{Physics->getTolerancesScale()};
        SceneDesc.gravity = physx::PxVec3{0.0f, 0.0f, -9.81f};
        SceneDesc.cpuDispatcher = Dispatcher;
        SceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

        if (!SceneDesc.isValid()) {
            MACAW_LOG(LogPhysics, Error, "Invalid PxSceneDesc.");

            return false;
        }

        mPhysXScene = Physics->createScene(SceneDesc);

        if (mPhysXScene == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX scene.");

            return false;
        }

        return true;
    }

    void FPhysicsScene::TermPhysScene() {
        if (mPhysXScene == nullptr) {
            return;
        }

        // 진행 중인 스텝을 끝낸 뒤 해제합니다.
        if (mPhysXSceneExecuting) {
            mPhysXScene->fetchResults(true);
            mPhysXSceneExecuting = false;
        }

        mPhysXScene->release();
        mPhysXScene = nullptr;
    }

    void FPhysicsScene::SetUpForFrame(const FVector& Gravity, float DeltaSeconds) {
        mDeltaSeconds = DeltaSeconds;

        if (mPhysXScene != nullptr) {
            mPhysXScene->setGravity(physx::PxVec3{Gravity.x, Gravity.y, Gravity.z});
        }
    }

    void FPhysicsScene::StartFrame() {
        if (mPhysXScene == nullptr) {
            return;
        }

        if (mPhysXSceneExecuting) {
            MACAW_LOG(LogPhysics, Log, "StartFrame: Already executing scene - aborting.");

            return;
        }

        // 프레임이 길어도 한 번에 1/30초 이상 진행하지 않습니다.
        const float UseDelta{std::min(mDeltaSeconds, mMaxPhysicsDeltaTime)};

        if (UseDelta <= 0.0f) {
            if (UseDelta < 0.0f) {
                MACAW_LOG(LogPhysics, Warning, "StartFrame: Negative timestep ({}) - aborting.", UseDelta);
            }

            return;
        }

        mPhysXSceneExecuting = true;
        mPhysXScene->simulate(UseDelta);
    }

    void FPhysicsScene::EndFrame() {
        if (mPhysXScene == nullptr || !mPhysXSceneExecuting) {
            return;
        }

        physx::PxU32 ErrorCode{};
        mPhysXScene->fetchResults(true, &ErrorCode);

        if (ErrorCode != 0) {
            MACAW_LOG(LogPhysics, Error, "fetchResults error: {}", ErrorCode);
        }

        mPhysXSceneExecuting = false;
    }

    physx::PxScene* FPhysicsScene::GetPxScene() const {
        return mPhysXScene;
    }
}
