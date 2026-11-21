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

        MACAW_LOG(LogPhysics, Log, "PhysX initialized.");

        return true;
    }

    void FPhysicsRuntime::Shutdown() {
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
}
