#pragma once

#include "PhysicsRuntime/FPhysicsScene.h"
#include "PhysicsRuntime/FPhysicsSettings.h"

#include <PxPhysicsAPI.h>
#include <memory>

namespace PhysicsRuntime {
    class FPhysicsRuntime {
    private:
        struct FRelease {
            template <typename ObjectType>
            void operator()(ObjectType* Object) const noexcept;
        };

    public:
        FPhysicsRuntime();
        ~FPhysicsRuntime();

    public:
        bool Initialize();
        void Shutdown();

        /**
         * @brief 월드 하나가 소유할 PhysX 씬을 만듭니다.
         * @details 반환한 씬은 이 런타임의 디스패처와 설정을 참조하므로 런타임보다 먼저 파괴해야 합니다.
         * @return 생성에 성공한 씬이며, 런타임이 초기화되지 않았거나 생성에 실패하면 nullptr입니다.
         */
        std::unique_ptr<FPhysicsScene> CreateScene();

        /**
         * @brief 모든 씬이 공유하는 시간 진행 설정을 반환합니다.
         */
        FPhysicsSettings& GetSettings();
        const FPhysicsSettings& GetSettings() const;

    private:
        physx::PxDefaultAllocator mAllocator{};
        physx::PxDefaultErrorCallback mErrorCallback{};

        std::unique_ptr<physx::PxFoundation, FRelease> mFoundation{};
        std::unique_ptr<physx::PxPhysics, FRelease> mPhysics{};
        std::unique_ptr<physx::PxDefaultCpuDispatcher, FRelease> mDispatcher{};

        FPhysicsSettings mSettings{};

        bool mExtensionsInitialized{};
    };

    template <typename ObjectType>
    void FPhysicsRuntime::FRelease::operator()(ObjectType* Object) const noexcept {
        Object->release();
    }
}
