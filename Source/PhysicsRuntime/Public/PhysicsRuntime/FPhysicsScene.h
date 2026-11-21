/**
 * @file FPhysicsScene.h
 * @brief 월드 하나의 PhysX 장면을 소유하고 한 스텝씩 진행하는 물리 장면을 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FVector.h"
#include <PxPhysicsAPI.h>

namespace PhysicsRuntime {
    class FPhysicsRuntime;

    /**
     * @brief 월드 하나의 PhysX 장면을 소유하고 한 스텝씩 진행합니다.
     * @ingroup PhysicsRuntime
     */
    class FPhysicsScene {
    public:
        /**
         * @brief 장면이 없는 빈 상태로 구성합니다.
         */
        FPhysicsScene();

        /**
         * @brief 진행 중인 스텝을 끝내고 소유한 PhysX 장면을 해제합니다.
         */
        ~FPhysicsScene();

        /**
         * @brief 장면 소유권이 중복되지 않도록 복사를 금지합니다.
         */
        FPhysicsScene(const FPhysicsScene&) = delete;

        /**
         * @brief 장면 소유권이 중복되지 않도록 대입을 금지합니다.
         */
        FPhysicsScene& operator=(const FPhysicsScene&) = delete;

    public:
        /**
         * @brief 런타임의 PhysX 객체로 장면을 생성합니다.
         * @param[in] Runtime 초기화된 PhysX 런타임입니다.
         * @return 장면을 생성했거나 이미 있으면 true, 실패하면 false입니다.
         */
        bool InitPhysScene(FPhysicsRuntime& Runtime);

        /**
         * @brief 진행 중인 스텝을 끝내고 장면을 해제합니다. 여러 번 호출해도 안전합니다.
         */
        void TermPhysScene();

        /**
         * @brief 이번 프레임의 중력과 시간을 설정합니다.
         * @param[in] Gravity 월드 중력 가속도이며 단위는 m/s²입니다.
         * @param[in] DeltaSeconds 이번 프레임의 초 단위 시간입니다.
         */
        void SetUpForFrame(const FVector& Gravity, float DeltaSeconds);

        /**
         * @brief 프레임 시간을 최대 스텝 시간으로 제한하여 시뮬레이션을 시작합니다.
         * @note 시간이 0 이하이거나 이미 진행 중이면 시작하지 않습니다.
         */
        void StartFrame();

        /**
         * @brief 시뮬레이션이 끝날 때까지 기다려 결과를 확정합니다.
         */
        void EndFrame();

        /**
         * @brief 소유한 PhysX 장면을 반환합니다.
         * @return PhysX 장면의 비소유 주소이며, 없으면 nullptr입니다.
         */
        physx::PxScene* GetPxScene() const;

    private:
        /**
         * @brief 소유한 PhysX 장면입니다.
         */
        physx::PxScene* mPhysXScene{};

        /**
         * @brief SetUpForFrame으로 받은 이번 프레임의 초 단위 시간입니다.
         */
        float mDeltaSeconds{};

        /**
         * @brief 한 스텝에 진행할 수 있는 최대 초 단위 시간입니다.
         */
        float mMaxPhysicsDeltaTime{1.0f / 30.0f};

        /**
         * @brief simulate를 호출한 뒤 fetchResults를 기다리는 중인지 나타냅니다.
         */
        bool mPhysXSceneExecuting{};
    };
}
