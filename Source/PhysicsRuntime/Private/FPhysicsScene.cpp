/**
 * @file FPhysicsScene.cpp
 * @brief 월드 하나의 PhysX 장면과 시뮬레이션 결과를 관리합니다.
 * @ingroup PhysicsRuntime
 */

#include "Core/Types.h"
#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/CollisionTypes.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"
#include "PhysicsRuntime/FVehicleManager.h"
#include "PhysicsRuntime/PhysicsConversion.h"
#include "PhysicsRuntime/PhysicsStats.h"
#include "Core/Log/Log.h"

#include <cmath>

namespace PhysicsRuntime {

    /**
     * @brief 장면 생성에 쓰는 충돌 필터와 접촉 수집 설정입니다.
     */
    namespace {
        DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

        /**
         * @brief 형상 쌍 하나에서 읽을 최대 접촉점 수입니다.
         */
        constexpr physx::PxU32 MaxContactPointsPerPair{8};

        /**
         * @brief 두 형상의 응답이 모두 Block인 쌍만 충돌시키고 새 접촉과 CCD 접촉을 알리도록 지정합니다.
         * @param[in] Attributes0 첫 번째 형상의 필터 속성입니다.
         * @param[in] FilterData0 첫 번째 형상의 필터 데이터이며, word0에 응답이 있습니다.
         * @param[in] Attributes1 두 번째 형상의 필터 속성입니다.
         * @param[in] FilterData1 두 번째 형상의 필터 데이터이며, word0에 응답이 있습니다.
         * @param[out] PairFlags 형상 쌍에 적용할 접촉 처리 플래그입니다.
         * @return Block 쌍이면 기본 필터 플래그, 아니면 접촉을 만들지 않는 억제 플래그입니다.
         * @note 겹침은 시뮬레이션이 아니라 장면 쿼리로 찾으므로, Overlap 쌍도 접촉을 만들지 않습니다.
         */
        physx::PxFilterFlags SimulationFilterShader(physx::PxFilterObjectAttributes Attributes0, physx::PxFilterData FilterData0, physx::PxFilterObjectAttributes Attributes1, physx::PxFilterData FilterData1, physx::PxPairFlags& PairFlags, const void*, physx::PxU32) {
            if (physx::PxFilterObjectIsTrigger(Attributes0) || physx::PxFilterObjectIsTrigger(Attributes1)) {
                PairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;

                return physx::PxFilterFlag::eDEFAULT;
            }

            const auto Response{PhysicsRuntime::CombineCollisionResponse(static_cast<PhysicsRuntime::ECollisionResponse>(FilterData0.word0), static_cast<PhysicsRuntime::ECollisionResponse>(FilterData1.word0))};

            if (Response != PhysicsRuntime::ECollisionResponse::Block) {
                return physx::PxFilterFlag::eSUPPRESS;
            }

            PairFlags = physx::PxPairFlag::eCONTACT_DEFAULT | physx::PxPairFlag::eDETECT_CCD_CONTACT | physx::PxPairFlag::eNOTIFY_TOUCH_FOUND | physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;

            return physx::PxFilterFlag::eDEFAULT;
        }
    }

    /**
     * @brief 장면이 없는 상태로 구성합니다.
     */
    PhysicsRuntime::FPhysicsScene::FPhysicsScene() = default;

    /**
     * @brief 생성한 PhysX 장면을 해제합니다.
     */
    PhysicsRuntime::FPhysicsScene::~FPhysicsScene() {
        Shutdown();
    }

    /**
     * @brief 런타임의 SDK 객체로 PhysX 장면을 만듭니다.
     */
    bool PhysicsRuntime::FPhysicsScene::Initialize(PhysicsRuntime::FPhysicsRuntime& Runtime) {
        if (IsInitialized()) {
            return true;
        }

        if (!Runtime.IsInitialized()) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a physics scene before the PhysX runtime is initialized.");

            return false;
        }

        const PhysicsRuntime::FPhysicsSettings& Settings{Runtime.GetSettings()};

        physx::PxSceneDesc Desc{Runtime.GetTolerancesScale()};
        Desc.gravity = PhysicsRuntime::ToPxVec3(Settings.mGravity);
        Desc.cpuDispatcher = Runtime.GetDispatcher();
        Desc.filterShader = SimulationFilterShader;
        Desc.simulationEventCallback = this;
        Desc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;

        if (Settings.mEnableCCD) {
            Desc.flags |= physx::PxSceneFlag::eENABLE_CCD;
        }

        if (Settings.mEnablePCM) {
            Desc.flags |= physx::PxSceneFlag::eENABLE_PCM;
        }

        if (!Desc.isValid()) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a physics scene from an invalid scene description.");

            return false;
        }

        mScene.reset(Runtime.GetPhysics()->createScene(Desc));

        if (mScene == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX scene.");

            return false;
        }

        if (physx::PxPvdSceneClient* PvdClient{mScene->getScenePvdClient()}) {
            PvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
            PvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
            PvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
        }

        mVehicleManager = std::make_unique<PhysicsRuntime::FVehicleManager>(*mScene, Runtime);
        mRuntime = &Runtime;

        return true;
    }

    /**
     * @brief PhysX 장면과 보관한 시뮬레이션 결과를 해제합니다.
     */
    void PhysicsRuntime::FPhysicsScene::Shutdown() {
        mContacts.clear();
        mVehicleManager.reset();
        mScene.reset();
        mRuntime = nullptr;
    }

    /**
     * @brief 장면을 만들 때 사용한 PhysX 런타임을 반환합니다.
     */
    PhysicsRuntime::FPhysicsRuntime* PhysicsRuntime::FPhysicsScene::GetRuntime() const {
        return mRuntime;
    }

    PhysicsRuntime::FVehicleManager* PhysicsRuntime::FPhysicsScene::GetVehicleManager() const {
        return mVehicleManager.get();
    }

    /**
     * @brief PhysX 장면이 준비되었는지 반환합니다.
     */
    bool PhysicsRuntime::FPhysicsScene::IsInitialized() const {
        return mScene != nullptr;
    }

    /**
     * @brief 장면을 한 스텝 진행하고 결과를 받을 때까지 기다립니다.
     */
    bool PhysicsRuntime::FPhysicsScene::Simulate(Core::FTime Step) {
        if (mScene == nullptr || !Step.IsFinite() || Step <= Core::FTime{}) {
            return false;
        }

        {
            const Core::FScopedStatTimer StatTimer{PhysicsRuntime::PhysicsStats::Simulate};

            mContacts.clear();
            mVehicleManager->Step(Step);
            mScene->simulate(Step.GetBase());
            mScene->fetchResults(true);
        }

        physx::PxU32 MovedCount{};
        mScene->getActiveActors(MovedCount);

        PhysicsRuntime::PhysicsStats::Steps.Add();
        PhysicsRuntime::PhysicsStats::MovedBodies.Add(static_cast<double>(MovedCount));
        PhysicsRuntime::PhysicsStats::Contacts.Add(static_cast<double>(mContacts.size()));
        PhysicsRuntime::PhysicsStats::DynamicBodies.Set(static_cast<double>(mScene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC)));
        PhysicsRuntime::PhysicsStats::StaticBodies.Set(static_cast<double>(mScene->getNbActors(physx::PxActorTypeFlag::eRIGID_STATIC)));

        return true;
    }

    /**
     * @brief PhysX 장면의 비소유 주소를 반환합니다.
     */
    physx::PxScene* PhysicsRuntime::FPhysicsScene::GetScene() const {
        return mScene.get();
    }

    /**
     * @brief 마지막 스텝에서 자세가 바뀐 강체 목록을 반환합니다.
     */
    std::span<physx::PxActor* const> PhysicsRuntime::FPhysicsScene::GetActiveActors() const {
        if (mScene == nullptr) {
            return {};
        }

        physx::PxU32 Count{};
        physx::PxActor** Actors{mScene->getActiveActors(Count)};

        return {Actors, Count};
    }

    /**
     * @brief 마지막 스텝에서 새로 시작된 접촉 목록을 반환합니다.
     */
    const Core::TArray<PhysicsRuntime::FPhysicsContact>& PhysicsRuntime::FPhysicsScene::GetContacts() const {
        return mContacts;
    }

    /**
     * @brief 새로 시작된 접촉을 접촉 목록에 기록합니다.
     * @details 형상 쌍마다 접촉점 위치를 평균하고 충격량을 합산하며, 법선은 첫 접촉점의 값을 사용합니다.
     */
    void PhysicsRuntime::FPhysicsScene::onContact(const physx::PxContactPairHeader& PairHeader, const physx::PxContactPair* Pairs, physx::PxU32 PairCount) {
        if (PairHeader.flags.isSet(physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0) || PairHeader.flags.isSet(physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1)) {
            return;
        }

        for (physx::PxU32 PairIndex{}; PairIndex < PairCount; ++PairIndex) {
            const physx::PxContactPair& Pair{Pairs[PairIndex]};

            if (!Pair.events.isSet(physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)) {
                continue;
            }

            physx::PxContactPairPoint Points[MaxContactPointsPerPair]{};
            const physx::PxU32 PointCount{Pair.extractContacts(Points, MaxContactPointsPerPair)};

            PhysicsRuntime::FPhysicsContact Contact{};
            Contact.mActor0 = PairHeader.actors[0];
            Contact.mActor1 = PairHeader.actors[1];

            if (PointCount > 0) {
                physx::PxVec3 PositionSum{physx::PxZero};
                physx::PxVec3 ImpulseSum{physx::PxZero};

                for (physx::PxU32 PointIndex{}; PointIndex < PointCount; ++PointIndex) {
                    PositionSum += Points[PointIndex].position;
                    ImpulseSum += Points[PointIndex].impulse;
                }

                Contact.mPoint = PhysicsRuntime::ToFVector(PositionSum / static_cast<physx::PxReal>(PointCount));
                Contact.mNormal = PhysicsRuntime::ToFVector(Points[0].normal);
                Contact.mImpulse = PhysicsRuntime::ToFVector(ImpulseSum);
            }

            mContacts.push_back(Contact);
        }
    }

    /**
     * @brief 트리거 이벤트는 아직 사용하지 않습니다.
     */
    void PhysicsRuntime::FPhysicsScene::onTrigger(physx::PxTriggerPair*, physx::PxU32) {
    }

    /**
     * @brief 조인트 파괴 이벤트는 아직 사용하지 않습니다.
     */
    void PhysicsRuntime::FPhysicsScene::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) {
    }

    /**
     * @brief 강체 깨어남 이벤트는 아직 사용하지 않습니다.
     */
    void PhysicsRuntime::FPhysicsScene::onWake(physx::PxActor**, physx::PxU32) {
    }

    /**
     * @brief 강체 잠듦 이벤트는 아직 사용하지 않습니다.
     */
    void PhysicsRuntime::FPhysicsScene::onSleep(physx::PxActor**, physx::PxU32) {
    }

    /**
     * @brief 시뮬레이션 도중 자세 전달 이벤트는 아직 사용하지 않습니다.
     */
    void PhysicsRuntime::FPhysicsScene::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) {
    }

}
