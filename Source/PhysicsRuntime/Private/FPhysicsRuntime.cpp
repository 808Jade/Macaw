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

    std::unique_ptr<FPhysicsScene> FPhysicsRuntime::CreateScene() {
        if (mPhysics == nullptr || mDispatcher == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create a physics scene before PhysX is initialized.");

            return nullptr;
        }

        auto Scene{std::make_unique<FPhysicsScene>(*mPhysics, *mDispatcher, mSettings)};

        if (!Scene->IsValid()) {
            return nullptr;
        }

        return Scene;
    }

    FPhysicsSettings& FPhysicsRuntime::GetSettings() {
        return mSettings;
    }

    const FPhysicsSettings& FPhysicsRuntime::GetSettings() const {
        return mSettings;
    }

    void FPhysicsRuntime::Shutdown() {
        // 씬은 디스패처와 PxPhysics를 참조하므로, 남아 있는 씬이 있으면 월드가 런타임보다 오래 살았다는 뜻입니다.
        if (mPhysics != nullptr && mPhysics->getNbScenes() > 0) {
            MACAW_LOG(LogPhysics, Error, "PhysX is shutting down while {} scene(s) are still alive.", mPhysics->getNbScenes());
        }

        mDispatcher.reset();

        if (mExtensionsInitialized) {
            PxCloseExtensions();
            mExtensionsInitialized = false;
        }

        mPhysics.reset();
        mFoundation.reset();
    }
}
