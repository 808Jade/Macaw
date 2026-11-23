/**
 * @file FBodyInstance.h
 * @brief 설계도로 PhysX 강체 하나를 만들어 물리 장면에 넣고 관리하는 바디를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Math/FTransform.h"
#include "Core/Types.h"
#include "PhysicsRuntime/PhysxUserData.h"

namespace physx {
    class PxRigidActor;
}

namespace PhysicsRuntime {
    class FPhysicsScene;
    class IPhysicsBodyOwner;
    class UBodySetup;

    /**
     * @brief 바디를 만들 때 강체 종류를 정하는 옵션입니다.
     * @ingroup PhysicsRuntime
     */
    struct FInitBodySpawnParams {
    public:
        /**
         * @brief true이면 고정된 강체를 만들고, false이면 움직이는 강체를 만듭니다.
         * @note 움직이는 강체가 시뮬레이션하지 않으면 코드로만 옮기는 키네마틱 강체가 됩니다.
         */
        bool mStaticPhysics{true};
    };

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
         * @param[in] SpawnParams 고정된 강체로 만들지 정하는 옵션입니다.
         * @return 강체를 만들어 장면에 넣었으면 true, 실패하면 false입니다.
         * @note 움직이는 강체는 mSimulatePhysics가 false이면 키네마틱으로 만듭니다.
         */
        bool InitBody(UBodySetup* Setup, const FTransform& Transform, IPhysicsBodyOwner* Owner, FPhysicsScene* Scene, const FInitBodySpawnParams& SpawnParams);

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

        /**
         * @brief 이미 만든 강체의 시뮬레이션 여부를 바꿉니다.
         * @param[in] Simulate true이면 물리로 움직이고, false이면 코드가 옮기는 키네마틱이 됩니다.
         * @note 고정된 강체는 바꿀 수 없으므로 mSimulatePhysics만 기록합니다.
         */
        void SetInstanceSimulatePhysics(bool Simulate);

        /**
         * @brief 강체를 지정한 월드 변환으로 옮깁니다.
         * @param[in] NewTransform 옮길 월드 변환이며 스케일은 무시합니다.
         * @note 키네마틱 강체는 다음 스텝 동안 그 위치로 이동하고, 그 밖의 강체는 바로 순간 이동합니다.
         */
        void SetBodyTransform(const FTransform& NewTransform);

        /**
         * @brief 강체의 현재 월드 변환을 반환합니다.
         * @return 위치와 회전이며 스케일은 1입니다. 강체가 없으면 기본 변환입니다.
         */
        FTransform GetUnrealWorldTransform() const;

    public:
        /**
         * @brief true이면 중력과 충돌로 움직이고, false이면 제자리에 고정됩니다.
         */
        bool mSimulatePhysics{};

        /**
         * @brief 물리 에셋 안에서 이 바디의 설계도 번호입니다.
         * @note InvalidIndex가 아니면 뼈 바디이므로 시뮬레이션 결과를 주인에게 바로 보내지 않습니다.
         */
        Index_t mInstanceBodyIndex{InvalidIndex};

        /**
         * @brief 이 바디가 붙은 뼈 번호입니다.
         */
        Index_t mInstanceBoneIndex{InvalidIndex};

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
