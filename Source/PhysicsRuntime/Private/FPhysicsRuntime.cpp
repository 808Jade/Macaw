#include "PhysicsRuntime/FPhysicsRuntime.h"
#include "Core/Log/Log.h"

namespace PhysicsRuntime {
    DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);

    FPhysicsRuntime::FPhysicsRuntime() = default;

    FPhysicsRuntime::~FPhysicsRuntime() {
        Shutdown();
    }

    bool FPhysicsRuntime::Initialize() {
        if (mDispatcher != nullptr) {
            return true;
        }

        mFoundation.reset(PxCreateFoundation(PX_PHYSICS_VERSION, mAllocator, mErrorCallback));

        if (mFoundation == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX foundation.");

            return false;
        }

        mPhysics.reset(PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, physx::PxTolerancesScale{}));

        if (mPhysics == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX SDK instance.");
            Shutdown();

            return false;
        }

        mExtensionsInitialized = PxInitExtensions(*mPhysics, nullptr);

        if (!mExtensionsInitialized) {
            MACAW_LOG(LogPhysics, Error, "Cannot initialize the PhysX extensions.");
            Shutdown();

            return false;
        }

        mDispatcher.reset(physx::PxDefaultCpuDispatcherCreate(1));

        if (mDispatcher == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX CPU dispatcher.");
            Shutdown();

            return false;
        }

        // 정지 마찰, 운동 마찰, 반발 계수 순서입니다.
        mDefaultMaterial.reset(mPhysics->createMaterial(0.7f, 0.7f, 0.3f));

        if (mDefaultMaterial == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the default PhysX material.");
            Shutdown();

            return false;
        }

        MACAW_LOG(LogPhysics, Log, "PhysX initialized.");

        return true;
    }

    void FPhysicsRuntime::Shutdown() {
        mDefaultMaterial.reset();
        mDispatcher.reset();

        if (mExtensionsInitialized) {
            PxCloseExtensions();
            mExtensionsInitialized = false;
        }

        mPhysics.reset();
        mFoundation.reset();
    }

    /**
     * @brief 물리 객체를 만드는 PhysX SDK를 반환합니다.
     */
    physx::PxPhysics* FPhysicsRuntime::GetPhysics() const {
        return mPhysics.get();
    }

    /**
     * @brief 물리 장면이 작업을 나눠 실행할 CPU 디스패처를 반환합니다.
     */
    physx::PxCpuDispatcher* FPhysicsRuntime::GetDispatcher() const {
        return mDispatcher.get();
    }

    /**
     * @brief 재질을 지정하지 않은 모양에 쓰는 기본 재질을 반환합니다.
     */
    physx::PxMaterial* FPhysicsRuntime::GetDefaultMaterial() const {
        return mDefaultMaterial.get();
    }
}
