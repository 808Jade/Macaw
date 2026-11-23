/**
 * @file FBodyInstance.cpp
 * @brief 설계도로 PhysX 강체를 만들고 질량을 계산해 물리 장면에 넣습니다.
 * @ingroup PhysicsRuntime
 */

#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/PhysXPublicCore.h"
#include "PhysicsRuntime/UBodySetup.h"
#include "Core/Log/Log.h"

#include <PxPhysicsAPI.h>
#include <algorithm>
#include <cmath>

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

    namespace {
        /**
         * @brief 물과 같은 기본 밀도이며 단위는 kg/m³입니다.
         */
        constexpr float DefaultDensity{1000.0f};

        /**
         * @brief 속이 빈 물체가 많은 점을 반영해 질량에 적용하는 지수입니다.
         */
        constexpr float RaiseMassToPower{0.75f};

        /**
         * @brief 강체 질량의 최솟값이며 단위는 kg입니다.
         */
        constexpr float MinimumMass{0.001f};
    }

    /**
     * @brief 강체가 없는 빈 바디를 구성합니다.
     */
    FBodyInstance::FBodyInstance() = default;

    /**
     * @brief 강체가 남아 있으면 장면에서 빼고 해제합니다.
     */
    FBodyInstance::~FBodyInstance() {
        TermBody();
    }

    /**
     * @brief 설계도로 강체를 만들어 물리 장면에 넣습니다.
     */
    bool FBodyInstance::InitBody(UBodySetup* Setup, const FTransform& Transform, IPhysicsBodyOwner* Owner, FPhysicsScene* Scene, const FInitBodySpawnParams& SpawnParams) {
        if (mActorHandle != nullptr) {
            return true;
        }

        if (Setup == nullptr || Setup->mAggGeom.GetElementCount() == 0 || Scene == nullptr || Scene->GetPxScene() == nullptr || Scene->GetDefaultMaterial() == nullptr) {
            return false;
        }

        physx::PxPhysics& Physics{PxGetPhysics()};
        const physx::PxTransform ActorTransform{U2PTransform(Transform)};

        if (SpawnParams.mStaticPhysics) {
            mActorHandle = Physics.createRigidStatic(ActorTransform);
        } else if (physx::PxRigidDynamic* Dynamic{Physics.createRigidDynamic(ActorTransform)}) {
            // 시뮬레이션하지 않는 움직이는 강체는 코드가 옮기는 위치로만 갑니다.
            Dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, !mSimulatePhysics);
            mActorHandle = Dynamic;
        }

        if (mActorHandle == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a PhysX rigid actor.");

            return false;
        }

        Setup->AddShapesToRigidActor_AssumesLocked(*mActorHandle, Transform.mScale, *Scene->GetDefaultMaterial());

        if (mActorHandle->getNbShapes() == 0) {
            MACAW_LOG(LogPhysics, Warning, "The body has no collision shapes.");
            mActorHandle->release();
            mActorHandle = nullptr;

            return false;
        }

        mBodySetup = Setup;
        mOwner = Owner;
        mPhysicsScene = Scene;

        UpdateMassProperties();

        mActorHandle->userData = &mPhysicsUserData;
        Scene->GetPxScene()->addActor(*mActorHandle);

        return true;
    }

    /**
     * @brief 강체를 장면에서 빼고 해제합니다.
     */
    void FBodyInstance::TermBody() {
        if (mActorHandle != nullptr) {
            // 해제 뒤 이 바디를 가리키지 않도록 꼬리표를 먼저 끊습니다.
            mActorHandle->userData = nullptr;
            mActorHandle->release();
            mActorHandle = nullptr;
        }

        mBodySetup = nullptr;
        mOwner = nullptr;
        mPhysicsScene = nullptr;
    }

    /**
     * @brief 강체가 만들어져 있는지 검사합니다.
     */
    bool FBodyInstance::IsValidBodyInstance() const {
        return mActorHandle != nullptr;
    }

    /**
     * @brief 바디의 PhysX 강체를 반환합니다.
     */
    physx::PxRigidActor* FBodyInstance::GetPhysicsActorHandle() const {
        return mActorHandle;
    }

    /**
     * @brief 바디의 주인을 반환합니다.
     */
    IPhysicsBodyOwner* FBodyInstance::GetOwner() const {
        return mOwner;
    }

    /**
     * @brief 강체 질량을 모양의 부피와 밀도로 계산해 설정합니다.
     */
    void FBodyInstance::UpdateMassProperties() {
        physx::PxRigidDynamic* Dynamic{mActorHandle != nullptr ? mActorHandle->is<physx::PxRigidDynamic>() : nullptr};

        if (Dynamic == nullptr) {
            return;
        }

        // 부피와 밀도로 질량, 질량 중심, 관성을 먼저 구합니다.
        physx::PxRigidBodyExt::updateMassAndInertia(*Dynamic, DefaultDensity);

        const float OldMass{Dynamic->getMass()};

        if (OldMass <= 0.0f) {
            return;
        }

        // 질량을 줄인 비율만큼 관성도 함께 줄입니다.
        const float NewMass{std::max(std::pow(OldMass, RaiseMassToPower), MinimumMass)};
        const float MassRatio{NewMass / OldMass};

        Dynamic->setMass(NewMass);
        Dynamic->setMassSpaceInertiaTensor(Dynamic->getMassSpaceInertiaTensor() * MassRatio);
    }
}
