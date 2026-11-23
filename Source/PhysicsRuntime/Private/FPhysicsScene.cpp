/**
 * @file FPhysicsScene.cpp
 * @brief 월드 하나의 PhysX 장면을 생성·진행·해제합니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"
#include "PhysicsRuntime/IPhysicsBodyOwner.h"
#include "PhysicsRuntime/PhysXPublicCore.h"
#include "PhysicsRuntime/PhysxUserData.h"
#include "Core/Log/Log.h"
#include "Core/Types.h"

#include <algorithm>
#include <utility>

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

    /**
     * @brief 장면이 없는 빈 상태로 구성합니다.
     */
    FPhysicsScene::FPhysicsScene() = default;

    /**
     * @brief 진행 중인 스텝을 끝내고 소유한 PhysX 장면을 해제합니다.
     */
    FPhysicsScene::~FPhysicsScene() {
        TermPhysScene();
    }

    /**
     * @brief 런타임의 PhysX 객체로 장면을 생성합니다.
     */
    bool FPhysicsScene::InitPhysScene(FPhysicsRuntime& Runtime) {
        if (mPhysXScene != nullptr) {
            return true;
        }

        physx::PxPhysics* Physics{Runtime.GetPhysics()};
        physx::PxCpuDispatcher* Dispatcher{Runtime.GetDispatcher()};
        physx::PxMaterial* DefaultMaterial{Runtime.GetDefaultMaterial()};

        if (Physics == nullptr || Dispatcher == nullptr || DefaultMaterial == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a physics scene before the PhysX runtime is initialized.");

            return false;
        }

        physx::PxSceneDesc SceneDesc{Physics->getTolerancesScale()};
        SceneDesc.gravity = physx::PxVec3{0.0f, 0.0f, -9.81f};
        SceneDesc.cpuDispatcher = Dispatcher;
        SceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
        // 스텝마다 움직인 액터 목록을 만들어 그 액터만 동기화합니다.
        SceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;

        if (!SceneDesc.isValid()) {
            MACAW_LOG(LogPhysics, Error, "Invalid PxSceneDesc.");

            return false;
        }

        mPhysXScene = Physics->createScene(SceneDesc);

        if (mPhysXScene == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX scene.");

            return false;
        }

        mDefaultMaterial = DefaultMaterial;

        return true;
    }

    /**
     * @brief 진행 중인 스텝을 끝내고 장면을 해제합니다.
     */
    void FPhysicsScene::TermPhysScene() {
        if (mPhysXScene == nullptr) {
            return;
        }

        // 계산 중인 장면을 해제하지 않도록 먼저 결과를 받습니다.
        if (mPhysXSceneExecuting) {
            mPhysXScene->fetchResults(true);
            mPhysXSceneExecuting = false;
        }

        mPhysXScene->release();
        mPhysXScene = nullptr;
        mDefaultMaterial = nullptr;
    }

    /**
     * @brief 이번 프레임의 중력과 시간을 설정합니다.
     */
    void FPhysicsScene::SetUpForFrame(const FVector& Gravity, float DeltaSeconds) {
        mDeltaSeconds = DeltaSeconds;

        if (mPhysXScene != nullptr) {
            mPhysXScene->setGravity(U2PVector(Gravity));
        }
    }

    /**
     * @brief 프레임 시간을 최대 스텝 시간으로 제한하여 시뮬레이션을 시작합니다.
     */
    void FPhysicsScene::StartFrame() {
        if (mPhysXScene == nullptr) {
            return;
        }

        if (mPhysXSceneExecuting) {
            MACAW_LOG(LogPhysics, Log, "StartFrame: Already executing scene - aborting.");

            return;
        }

        // 프레임이 길어도 한 번에 최대 스텝 시간보다 많이 진행하지 않습니다.
        const float UseDelta{std::min(mDeltaSeconds, mMaxPhysicsDeltaTime)};

        if (UseDelta <= 0.0f) {
            // 일시정지로 인한 0은 조용히 건너뜁니다.
            if (UseDelta < 0.0f) {
                MACAW_LOG(LogPhysics, Warning, "StartFrame: Negative timestep ({}) - aborting.", UseDelta);
            }

            return;
        }

        mPhysXSceneExecuting = true;
        mPhysXScene->simulate(UseDelta);
    }

    /**
     * @brief 시뮬레이션이 끝날 때까지 기다려 결과를 확정합니다.
     */
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

        SyncComponentsToBodies();
    }

    /**
     * @brief 이번 스텝에 움직인 바디의 위치와 회전을 주인에게 알립니다.
     */
    void FPhysicsScene::SyncComponentsToBodies() {
        physx::PxU32 ActiveActorCount{};
        physx::PxActor** ActiveActors{mPhysXScene->getActiveActors(ActiveActorCount)};

        // 주인을 옮기다가 다른 바디가 해제될 수 있으므로 먼저 목록만 모읍니다.
        Core::TArray<std::pair<IPhysicsBodyOwner*, FTransform>> PendingTransforms{};
        PendingTransforms.reserve(ActiveActorCount);

        for (physx::PxU32 Index{}; Index < ActiveActorCount; ++Index) {
            const physx::PxRigidActor* RigidActor{ActiveActors[Index]->is<physx::PxRigidActor>()};

            if (RigidActor == nullptr) {
                continue;
            }

            const FBodyInstance* Body{FPhysxUserData::Get<FBodyInstance>(RigidActor->userData)};

            // 뼈 바디는 주인 전체를 옮기지 않고 스켈레탈 쪽에서 포즈로 따로 읽습니다.
            if (Body != nullptr && Body->GetOwner() != nullptr && Body->mInstanceBodyIndex == InvalidIndex) {
                PendingTransforms.emplace_back(Body->GetOwner(), P2UTransform(RigidActor->getGlobalPose()));
            }
        }

        for (const auto& [Owner, Transform] : PendingTransforms) {
            Owner->OnPhysicsTransformUpdated(Transform);
        }
    }

    /**
     * @brief 소유한 PhysX 장면을 반환합니다.
     */
    physx::PxScene* FPhysicsScene::GetPxScene() const {
        return mPhysXScene;
    }

    /**
     * @brief 재질을 지정하지 않은 모양에 쓰는 기본 재질을 반환합니다.
     */
    physx::PxMaterial* FPhysicsScene::GetDefaultMaterial() const {
        return mDefaultMaterial;
    }
}
