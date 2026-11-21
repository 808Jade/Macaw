/**
 * @file FPhysicsScene.h
 * @brief 월드 하나의 PhysX 장면과 시뮬레이션 결과를 관리합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "PhysicsRuntime/FPhysicsContact.h"
#include "PhysicsRuntime/FPhysXRelease.h"

#include <PxPhysicsAPI.h>
#include <memory>
#include <span>

namespace PhysicsRuntime {

    class FPhysicsRuntime;

    /**
     * @brief 월드 하나의 PhysX 장면과 시뮬레이션 결과를 관리합니다.
     * @details 런타임이 만든 SDK 객체와 디스패처를 빌려 장면을 만들고, 한 스텝씩 동기로 시뮬레이션합니다.
     *          스텝마다 움직인 강체 목록과 새로 시작된 접촉을 보관하며, 장면 이벤트를 직접 수신합니다.
     * @note 이 객체에서 빌린 런타임보다 먼저 해제되어야 하며, 장면에 추가한 강체는 장면을 해제하기 전에 제거해야 합니다.
     * @ingroup PhysicsRuntime
     */
    class FPhysicsScene final : private physx::PxSimulationEventCallback {
    public:

        /**
         * @brief 장면이 없는 상태로 구성합니다.
         */
        FPhysicsScene();

        /**
         * @brief 생성한 PhysX 장면을 해제합니다.
         */
        ~FPhysicsScene() override;

        /**
         * @brief 장면이 이벤트 수신자로 자신의 주소를 등록하므로 복사하지 않습니다.
         */
        FPhysicsScene(const FPhysicsScene&) = delete;

        /**
         * @brief 장면이 이벤트 수신자로 자신의 주소를 등록하므로 복사하지 않습니다.
         */
        FPhysicsScene& operator=(const FPhysicsScene&) = delete;

    public:

        /**
         * @brief 런타임의 SDK 객체로 PhysX 장면을 만듭니다.
         * @param[in] Runtime 초기화를 마친 PhysX 런타임입니다.
         * @return 이미 초기화되었거나 장면 생성에 성공하면 true, 그렇지 않으면 false입니다.
         */
        bool Initialize(PhysicsRuntime::FPhysicsRuntime& Runtime);

        /**
         * @brief PhysX 장면과 보관한 시뮬레이션 결과를 해제합니다.
         * @note 여러 번 호출해도 안전합니다.
         */
        void Shutdown();

        /**
         * @brief PhysX 장면이 준비되었는지 반환합니다.
         * @return 장면이 있으면 true입니다.
         */
        bool IsInitialized() const;

        /**
         * @brief 장면을 한 스텝 진행하고 결과를 받을 때까지 기다립니다.
         * @param[in] StepSeconds 진행할 시간(초)이며 양의 유한값이어야 합니다.
         * @return 시뮬레이션을 진행했으면 true, 장면이 없거나 시간이 유효하지 않으면 false입니다.
         * @note 이전 스텝의 접촉 정보는 지우고 이번 스텝의 결과로 바꿉니다.
         */
        bool Simulate(float StepSeconds);

        /**
         * @brief PhysX 장면의 비소유 주소를 반환합니다.
         * @return 조회한 값의 비소유 주소이며, 초기화 전이면 nullptr입니다.
         */
        physx::PxScene* GetScene() const;

        /**
         * @brief 마지막 스텝에서 자세가 바뀐 강체 목록을 반환합니다.
         * @return 장면이 소유한 목록이며 다음 시뮬레이션 전까지 유효합니다. 장면이 없으면 빈 목록입니다.
         */
        std::span<physx::PxActor* const> GetActiveActors() const;

        /**
         * @brief 마지막 스텝에서 새로 시작된 접촉 목록을 반환합니다.
         * @return 이 객체가 보관하는 값의 참조이며 다음 시뮬레이션 전까지 유효합니다.
         */
        const Core::TArray<PhysicsRuntime::FPhysicsContact>& GetContacts() const;

    private:

        /**
         * @brief 새로 시작된 접촉을 접촉 목록에 기록합니다.
         * @param[in] PairHeader 접촉한 두 강체의 정보입니다.
         * @param[in] Pairs 두 강체 사이의 형상 쌍별 접촉 정보입니다.
         * @param[in] PairCount 형상 쌍의 개수입니다.
         */
        void onContact(const physx::PxContactPairHeader& PairHeader, const physx::PxContactPair* Pairs, physx::PxU32 PairCount) override;

        /**
         * @brief 트리거 이벤트는 아직 사용하지 않습니다.
         */
        void onTrigger(physx::PxTriggerPair*, physx::PxU32) override;

        /**
         * @brief 조인트 파괴 이벤트는 아직 사용하지 않습니다.
         */
        void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override;

        /**
         * @brief 강체 깨어남 이벤트는 아직 사용하지 않습니다.
         */
        void onWake(physx::PxActor**, physx::PxU32) override;

        /**
         * @brief 강체 잠듦 이벤트는 아직 사용하지 않습니다.
         */
        void onSleep(physx::PxActor**, physx::PxU32) override;

        /**
         * @brief 시뮬레이션 도중 자세 전달 이벤트는 아직 사용하지 않습니다.
         */
        void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) override;

    private:

        /**
         * @brief 이 객체가 단독 소유하는 PhysX 장면입니다.
         */
        std::unique_ptr<physx::PxScene, PhysicsRuntime::FPhysXRelease> mScene{};

        /**
         * @brief 마지막 스텝에서 새로 시작된 접촉 목록입니다.
         */
        Core::TArray<PhysicsRuntime::FPhysicsContact> mContacts{};
    };

}
