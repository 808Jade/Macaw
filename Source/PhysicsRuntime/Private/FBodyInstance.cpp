/**
 * @file FBodyInstance.cpp
 * @brief 컴포넌트 하나에 대응하는 PhysX 강체와 그 시뮬레이션 설정을 관리합니다.
 * @ingroup PhysicsRuntime
 */

#include "Core/Types.h"
#include "PhysicsRuntime/FBodyInstance.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"
#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/PhysicsConversion.h"
#include "PhysicsRuntime/UBodySetup.h"
#include "Core/Log/Log.h"

#include <PxPhysicsAPI.h>

#include <cmath>

namespace PhysicsRuntime {

    /**
     * @brief 바디 생성과 설정 검증에 쓰는 로그 범주와 보조 함수입니다.
     */
    namespace {
        DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

        /**
         * @brief 질량과 감쇠 계수로 쓸 수 있는 값인지 검사합니다.
         * @param[in] Value 검사할 값입니다.
         * @return 0 이상의 유한값이면 true입니다.
         */
        bool IsNonNegativeFinite(float Value) {
            return std::isfinite(Value) && Value >= 0.0f;
        }
    }

    /**
     * @brief 실제 바디가 없는 기본 설정의 바디 인스턴스를 구성합니다.
     */
    PhysicsRuntime::FBodyInstance::FBodyInstance() = default;

    /**
     * @brief 다른 바디 인스턴스의 설정만 복사합니다.
     */
    PhysicsRuntime::FBodyInstance::FBodyInstance(const FBodyInstance& Other)
        : mSimulatePhysics{Other.mSimulatePhysics}
        , mEnableGravity{Other.mEnableGravity}
        , mUseCCD{Other.mUseCCD}
        , mMassInKg{Other.mMassInKg}
        , mLinearDamping{Other.mLinearDamping}
        , mAngularDamping{Other.mAngularDamping} {
    }

    /**
     * @brief 이 객체의 실제 바디를 해제하고 다른 바디 인스턴스의 설정만 복사합니다.
     */
    PhysicsRuntime::FBodyInstance& PhysicsRuntime::FBodyInstance::operator=(const FBodyInstance& Other) {
        if (this == &Other) {
            return *this;
        }

        TermBody();

        mSimulatePhysics = Other.mSimulatePhysics;
        mEnableGravity = Other.mEnableGravity;
        mUseCCD = Other.mUseCCD;
        mMassInKg = Other.mMassInKg;
        mLinearDamping = Other.mLinearDamping;
        mAngularDamping = Other.mAngularDamping;

        return *this;
    }

    /**
     * @brief 실제 바디를 해제합니다.
     */
    PhysicsRuntime::FBodyInstance::~FBodyInstance() {
        TermBody();
    }

    /**
     * @brief 바디 셋업의 충돌 형상으로 PhysX 강체를 만들어 장면에 추가합니다.
     */
    bool PhysicsRuntime::FBodyInstance::InitBody(const PhysicsRuntime::UBodySetup& Setup, const FTransform& Transform, Engine::UPrimitiveComponent* OwnerComponent, PhysicsRuntime::FPhysicsScene& Scene) {
        TermBody();

        PhysicsRuntime::FPhysicsRuntime* Runtime{Scene.GetRuntime()};

        if (!Scene.IsInitialized() || Runtime == nullptr) {
            MACAW_LOG(LogPhysics, Warning, "Cannot create a body for '{}' without an initialized physics scene.", Setup.GetName());

            return false;
        }

        if (Setup.GetAggGeom().IsEmpty()) {
            return false;
        }

        const physx::PxTransform Pose{PhysicsRuntime::ToPxTransform(Transform)};

        if (!Pose.isValid()) {
            MACAW_LOG(LogPhysics, Warning, "Cannot create a body for '{}' from an invalid transform.", Setup.GetName());

            return false;
        }

        physx::PxPhysics& Physics{*Runtime->GetPhysics()};
        std::unique_ptr<physx::PxRigidActor, PhysicsRuntime::FPhysXRelease> Actor{};

        if (mSimulatePhysics) {
            Actor.reset(Physics.createRigidDynamic(Pose));
        } else {
            Actor.reset(Physics.createRigidStatic(Pose));
        }

        if (Actor == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a PhysX rigid actor for '{}'.", Setup.GetName());

            return false;
        }

        if (Setup.AddShapesToRigidActor(*Actor, Transform.mScale, *Runtime->GetDefaultMaterial()) == 0) {
            MACAW_LOG(LogPhysics, Warning, "Body setup '{}' has no valid collision shape at scale ({}, {}, {}).", Setup.GetName(), Transform.mScale.x, Transform.mScale.y, Transform.mScale.z);

            return false;
        }

        Actor->userData = this;
        mActor = std::move(Actor);
        mScene = &Scene;
        mOwnerComponent = OwnerComponent;
        mBodyScale = Transform.mScale;

        if (physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()}) {
            ApplyDynamicSettings(*Dynamic);
        }

        Scene.GetScene()->addActor(*mActor);

        return true;
    }

    /**
     * @brief 실제 바디를 장면에서 제거하고 해제합니다.
     */
    void PhysicsRuntime::FBodyInstance::TermBody() {
        mActor.reset();
        mScene = nullptr;
        mOwnerComponent = nullptr;
    }

    /**
     * @brief 실제 바디가 있는지 반환합니다.
     */
    bool PhysicsRuntime::FBodyInstance::IsValidBodyInstance() const {
        return mActor != nullptr;
    }

    /**
     * @brief 실제 바디가 시뮬레이션되는 동적 강체인지 반환합니다.
     */
    bool PhysicsRuntime::FBodyInstance::IsDynamic() const {
        return GetPxRigidDynamic() != nullptr;
    }

    /**
     * @brief 바디를 소유하는 컴포넌트를 반환합니다.
     */
    Engine::UPrimitiveComponent* PhysicsRuntime::FBodyInstance::GetOwnerComponent() const {
        return mOwnerComponent;
    }

    /**
     * @brief PhysX 강체를 반환합니다.
     */
    physx::PxRigidActor* PhysicsRuntime::FBodyInstance::GetPxRigidActor() const {
        return mActor.get();
    }

    /**
     * @brief 동적 PhysX 강체를 반환합니다.
     */
    physx::PxRigidDynamic* PhysicsRuntime::FBodyInstance::GetPxRigidDynamic() const {
        return mActor != nullptr ? mActor->is<physx::PxRigidDynamic>() : nullptr;
    }

    /**
     * @brief 바디의 현재 월드 트랜스폼을 반환합니다.
     */
    FTransform PhysicsRuntime::FBodyInstance::GetBodyTransform() const {
        if (mActor == nullptr) {
            return FTransform{};
        }

        return PhysicsRuntime::ToFTransform(mActor->getGlobalPose(), mBodyScale);
    }

    /**
     * @brief 형상 크기에 반영한 스케일을 반환합니다.
     */
    const FVector& PhysicsRuntime::FBodyInstance::GetBodyScale() const {
        return mBodyScale;
    }

    /**
     * @brief 바디를 지정한 위치·회전으로 순간 이동합니다.
     */
    bool PhysicsRuntime::FBodyInstance::SetBodyTransform(const FTransform& Transform) {
        if (mActor == nullptr) {
            return false;
        }

        const physx::PxTransform Pose{PhysicsRuntime::ToPxTransform(Transform)};

        if (!Pose.isValid()) {
            return false;
        }

        mActor->setGlobalPose(Pose);

        return true;
    }

    /**
     * @brief 동적 강체의 질량 중심에 힘을 더합니다.
     */
    bool PhysicsRuntime::FBodyInstance::AddForce(const FVector& Force, bool AccelChange) {
        physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()};
        const physx::PxVec3 Value{PhysicsRuntime::ToPxVec3(Force)};

        if (Dynamic == nullptr || !Value.isFinite()) {
            return false;
        }

        Dynamic->addForce(Value, AccelChange ? physx::PxForceMode::eACCELERATION : physx::PxForceMode::eFORCE);

        return true;
    }

    /**
     * @brief 동적 강체의 질량 중심에 충격량을 더합니다.
     */
    bool PhysicsRuntime::FBodyInstance::AddImpulse(const FVector& Impulse, bool VelocityChange) {
        physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()};
        const physx::PxVec3 Value{PhysicsRuntime::ToPxVec3(Impulse)};

        if (Dynamic == nullptr || !Value.isFinite()) {
            return false;
        }

        Dynamic->addForce(Value, VelocityChange ? physx::PxForceMode::eVELOCITY_CHANGE : physx::PxForceMode::eIMPULSE);

        return true;
    }

    /**
     * @brief 동적 강체의 선속도를 반환합니다.
     */
    FVector PhysicsRuntime::FBodyInstance::GetLinearVelocity() const {
        const physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()};

        return Dynamic != nullptr ? PhysicsRuntime::ToFVector(Dynamic->getLinearVelocity()) : FVector{};
    }

    /**
     * @brief 동적 강체의 선속도를 설정합니다.
     */
    bool PhysicsRuntime::FBodyInstance::SetLinearVelocity(const FVector& Velocity) {
        physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()};
        const physx::PxVec3 Value{PhysicsRuntime::ToPxVec3(Velocity)};

        if (Dynamic == nullptr || !Value.isFinite()) {
            return false;
        }

        Dynamic->setLinearVelocity(Value);

        return true;
    }

    /**
     * @brief 동적 강체의 각속도를 반환합니다.
     */
    FVector PhysicsRuntime::FBodyInstance::GetAngularVelocity() const {
        const physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()};

        return Dynamic != nullptr ? PhysicsRuntime::ToFVector(Dynamic->getAngularVelocity()) : FVector{};
    }

    /**
     * @brief 동적 강체의 각속도를 설정합니다.
     */
    bool PhysicsRuntime::FBodyInstance::SetAngularVelocity(const FVector& Velocity) {
        physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()};
        const physx::PxVec3 Value{PhysicsRuntime::ToPxVec3(Velocity)};

        if (Dynamic == nullptr || !Value.isFinite()) {
            return false;
        }

        Dynamic->setAngularVelocity(Value);

        return true;
    }

    /**
     * @brief PhysX 강체에서 그 강체를 만든 바디 인스턴스를 찾습니다.
     */
    PhysicsRuntime::FBodyInstance* PhysicsRuntime::FBodyInstance::FromPxActor(const physx::PxActor& Actor) {
        return static_cast<FBodyInstance*>(Actor.userData);
    }

    /**
     * @brief 바디를 만들 때 동적 강체로 시뮬레이션할지 여부를 반환합니다.
     */
    bool PhysicsRuntime::FBodyInstance::ShouldSimulatePhysics() const {
        return mSimulatePhysics;
    }

    /**
     * @brief 바디를 만들 때 동적 강체로 시뮬레이션할지 설정합니다.
     */
    void PhysicsRuntime::FBodyInstance::SetSimulatePhysics(bool Simulate) {
        mSimulatePhysics = Simulate;
    }

    /**
     * @brief 동적 강체에 중력을 적용할지 반환합니다.
     */
    bool PhysicsRuntime::FBodyInstance::IsGravityEnabled() const {
        return mEnableGravity;
    }

    /**
     * @brief 동적 강체에 중력을 적용할지 설정하고 실제 바디에 반영합니다.
     */
    void PhysicsRuntime::FBodyInstance::SetGravityEnabled(bool Enabled) {
        mEnableGravity = Enabled;

        if (physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()}) {
            Dynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !mEnableGravity);
            Dynamic->wakeUp();
        }
    }

    /**
     * @brief 동적 강체에 연속 충돌 검출(CCD)을 사용할지 반환합니다.
     */
    bool PhysicsRuntime::FBodyInstance::IsCCDEnabled() const {
        return mUseCCD;
    }

    /**
     * @brief 동적 강체에 연속 충돌 검출(CCD)을 사용할지 설정하고 실제 바디에 반영합니다.
     */
    void PhysicsRuntime::FBodyInstance::SetCCDEnabled(bool Enabled) {
        mUseCCD = Enabled;

        if (physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()}) {
            Dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, mUseCCD);
        }
    }

    /**
     * @brief 동적 강체의 지정 질량을 반환합니다.
     */
    float PhysicsRuntime::FBodyInstance::GetMassInKg() const {
        return mMassInKg;
    }

    /**
     * @brief 동적 강체의 질량을 설정하고 실제 바디의 질량과 관성을 다시 계산합니다.
     */
    bool PhysicsRuntime::FBodyInstance::SetMassInKg(float MassInKg) {
        if (!IsNonNegativeFinite(MassInKg)) {
            return false;
        }

        mMassInKg = MassInKg;

        if (physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()}) {
            UpdateMassProperties(*Dynamic);
        }

        return true;
    }

    /**
     * @brief 동적 강체의 선형 감쇠 계수를 반환합니다.
     */
    float PhysicsRuntime::FBodyInstance::GetLinearDamping() const {
        return mLinearDamping;
    }

    /**
     * @brief 동적 강체의 선형 감쇠 계수를 설정하고 실제 바디에 반영합니다.
     */
    bool PhysicsRuntime::FBodyInstance::SetLinearDamping(float Damping) {
        if (!IsNonNegativeFinite(Damping)) {
            return false;
        }

        mLinearDamping = Damping;

        if (physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()}) {
            Dynamic->setLinearDamping(mLinearDamping);
        }

        return true;
    }

    /**
     * @brief 동적 강체의 회전 감쇠 계수를 반환합니다.
     */
    float PhysicsRuntime::FBodyInstance::GetAngularDamping() const {
        return mAngularDamping;
    }

    /**
     * @brief 동적 강체의 회전 감쇠 계수를 설정하고 실제 바디에 반영합니다.
     */
    bool PhysicsRuntime::FBodyInstance::SetAngularDamping(float Damping) {
        if (!IsNonNegativeFinite(Damping)) {
            return false;
        }

        mAngularDamping = Damping;

        if (physx::PxRigidDynamic* Dynamic{GetPxRigidDynamic()}) {
            Dynamic->setAngularDamping(mAngularDamping);
        }

        return true;
    }

    /**
     * @brief 질량·감쇠·중력·CCD 설정을 동적 강체에 반영합니다.
     */
    void PhysicsRuntime::FBodyInstance::ApplyDynamicSettings(physx::PxRigidDynamic& Dynamic) const {
        Dynamic.setLinearDamping(mLinearDamping);
        Dynamic.setAngularDamping(mAngularDamping);
        Dynamic.setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !mEnableGravity);
        Dynamic.setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, mUseCCD);
        UpdateMassProperties(Dynamic);
    }

    /**
     * @brief 질량 설정으로 동적 강체의 질량과 관성을 다시 계산합니다.
     */
    void PhysicsRuntime::FBodyInstance::UpdateMassProperties(physx::PxRigidDynamic& Dynamic) const {
        bool Updated{};

        if (mMassInKg > 0.0f) {
            Updated = physx::PxRigidBodyExt::setMassAndUpdateInertia(Dynamic, mMassInKg);
        } else if (mScene != nullptr && mScene->GetRuntime() != nullptr) {
            Updated = physx::PxRigidBodyExt::updateMassAndInertia(Dynamic, mScene->GetRuntime()->GetSettings().mDefaultDensity);
        }

        if (!Updated) {
            MACAW_LOG(LogPhysics, Warning, "Cannot update the mass properties of a dynamic body.");
        }
    }

}
