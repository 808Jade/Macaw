/**
 * @file FBodyInstance.h
 * @brief 설계도로 PhysX 강체 하나를 만들어 물리 장면에 넣고 관리하는 바디를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FTransform.h"
#include "PhysicsRuntime/PhysxUserData.h"

namespace physx {
    class PxRigidActor;
}

namespace PhysicsRuntime {
    class FPhysicsScene;
    class IPhysicsBodyOwner;
    class UBodySetup;

    /**
     * @brief 설계도로 PhysX 강체 하나를 만들어 물리 장면에 넣고 관리합니다.
     * @ingroup PhysicsRuntime
     * @note 강체의 userData가 이 객체를 가리키므로 복사와 이동을 금지합니다.
     */
    class FBodyInstance {
    public:
        /**
         * @brief 강체가 없는 빈 바디를 구성합니다.
         */
        FBodyInstance();

        /**
         * @brief 강체가 남아 있으면 장면에서 빼고 해제합니다.
         */
        ~FBodyInstance();

        /**
         * @brief userData가 가리키는 주소가 바뀌지 않도록 복사를 금지합니다.
         */
        FBodyInstance(const FBodyInstance&) = delete;

        /**
         * @brief userData가 가리키는 주소가 바뀌지 않도록 대입을 금지합니다.
         */
        FBodyInstance& operator=(const FBodyInstance&) = delete;

    public:
        /**
         * @brief 설계도로 강체를 만들어 물리 장면에 넣습니다.
         * @param[in] Setup 모양 목록을 가진 설계도입니다.
         * @param[in] Transform 바디의 월드 변환이며 스케일은 모양 크기에 반영합니다.
         * @param[in] Owner 물리 결과를 받을 주인입니다.
         * @param[in] Scene 강체를 넣을 물리 장면입니다.
         * @return 강체를 만들어 장면에 넣었으면 true, 실패하면 false입니다.
         * @note mSimulatePhysics가 true이면 움직이는 강체, false이면 고정된 강체를 만듭니다.
         */
        bool InitBody(UBodySetup* Setup, const FTransform& Transform, IPhysicsBodyOwner* Owner, FPhysicsScene* Scene);

        /**
         * @brief 강체를 장면에서 빼고 해제합니다. 여러 번 호출해도 안전합니다.
         */
        void TermBody();

        /**
         * @brief 강체가 만들어져 있는지 검사합니다.
         * @return 강체가 있으면 true입니다.
         */
        bool IsValidBodyInstance() const;

        /**
         * @brief 바디의 PhysX 강체를 반환합니다.
         * @return 강체의 비소유 주소이며, 없으면 nullptr입니다.
         */
        physx::PxRigidActor* GetPhysicsActorHandle() const;

        /**
         * @brief 바디의 주인을 반환합니다.
         * @return 주인의 비소유 주소이며, 없으면 nullptr입니다.
         */
        IPhysicsBodyOwner* GetOwner() const;

    public:
        /**
         * @brief true이면 중력과 충돌로 움직이고, false이면 제자리에 고정됩니다.
         */
        bool mSimulatePhysics{};

    private:
        /**
         * @brief 강체 질량을 모양의 부피와 밀도로 계산해 설정합니다.
         */
        void UpdateMassProperties();

    private:
        /**
         * @brief 소유한 PhysX 강체입니다.
         */
        physx::PxRigidActor* mActorHandle{};

        /**
         * @brief 강체를 만든 설계도의 비소유 주소입니다.
         */
        UBodySetup* mBodySetup{};

        /**
         * @brief 물리 결과를 받을 주인의 비소유 주소입니다.
         */
        IPhysicsBodyOwner* mOwner{};

        /**
         * @brief 강체를 넣은 물리 장면의 비소유 주소입니다.
         */
        FPhysicsScene* mPhysicsScene{};

        /**
         * @brief 강체의 userData가 가리키는 이 바디의 꼬리표입니다.
         */
        FPhysxUserData mPhysicsUserData{this};
    };
}
