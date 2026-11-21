#pragma once

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
         * @brief 물리 객체를 만드는 PhysX SDK를 반환합니다.
         * @return PhysX SDK의 비소유 주소이며, 초기화 전이면 nullptr입니다.
         */
        physx::PxPhysics* GetPhysics() const;

        /**
         * @brief 물리 장면이 작업을 나눠 실행할 CPU 디스패처를 반환합니다.
         * @return 디스패처의 비소유 주소이며, 초기화 전이면 nullptr입니다.
         */
        physx::PxCpuDispatcher* GetDispatcher() const;

    private:
        physx::PxDefaultAllocator mAllocator{};
        physx::PxDefaultErrorCallback mErrorCallback{};

        std::unique_ptr<physx::PxFoundation, FRelease> mFoundation{};
        std::unique_ptr<physx::PxPhysics, FRelease> mPhysics{};
        std::unique_ptr<physx::PxDefaultCpuDispatcher, FRelease> mDispatcher{};

        bool mExtensionsInitialized{};
    };

    template <typename ObjectType>
    void FPhysicsRuntime::FRelease::operator()(ObjectType* Object) const noexcept {
        Object->release();
    }
}
