/**
 * @file FPhysicsRuntime.cpp
 * @brief 프로세스 전체에서 공유하는 PhysX SDK 객체의 수명을 관리합니다.
 * @ingroup PhysicsRuntime
 */

#include "Core/Types.h"
#include "PhysicsRuntime/FPhysicsRuntime.h"
#include "Core/Literals.h"
#include "Core/Log/Log.h"

using namespace Core::Literals;

namespace PhysicsRuntime {

    /**
     * @brief PhysX 런타임의 고정 설정과 로그 범주입니다.
     */
    namespace {
        /**
         * @brief 엔진 1유닛에 대응하는 실제 길이입니다.
         */
        constexpr Core::FLength UnitLength{1_m};

        /**
         * @brief 허용 오차의 속도 기준으로 사용하는 표준 중력 가속도입니다.
         */
        constexpr Core::FAcceleration StandardGravity{9.81_mps2};

        /**
         * @brief 시뮬레이션 작업을 실행할 CPU 디스패처의 작업 스레드 수입니다.
         */
        constexpr physx::PxU32 DispatcherThreadCount{2};

        /**
         * @brief 기본 재질의 정지 마찰 계수입니다.
         */
        constexpr physx::PxReal DefaultStaticFriction{0.5f};

        /**
         * @brief 기본 재질의 운동 마찰 계수입니다.
         */
        constexpr physx::PxReal DefaultDynamicFriction{0.5f};

        /**
         * @brief 기본 재질의 반발 계수입니다.
         */
        constexpr physx::PxReal DefaultRestitution{0.6f};

        /**
         * @brief PVD 프로그램이 대기하는 호스트 주소입니다.
         */
        constexpr const char* PvdHost{"127.0.0.1"};

        /**
         * @brief PVD 프로그램이 대기하는 기본 포트입니다.
         */
        constexpr int PvdPort{5425};

        /**
         * @brief PVD 연결을 기다리는 최대 시간(밀리초)입니다.
         */
        constexpr unsigned int PvdConnectTimeoutMilliseconds{10};

        DEFINE_LOG_CATEGORY_STATIC(LogPhysics, Log, All);
    }

    /**
     * @brief 초기화하지 않은 런타임을 구성합니다.
     */
    PhysicsRuntime::FPhysicsRuntime::FPhysicsRuntime() = default;

    /**
     * @brief 생성한 PhysX 객체를 모두 해제합니다.
     */
    PhysicsRuntime::FPhysicsRuntime::~FPhysicsRuntime() {
        Shutdown();
    }

    /**
     * @brief PhysX SDK 객체를 생성하고 PVD 연결을 시도합니다.
     */
    bool PhysicsRuntime::FPhysicsRuntime::Initialize() {
        if (IsInitialized()) {
            return true;
        }

        mTolerancesScale.length = UnitLength / 1_m;
        mTolerancesScale.speed = StandardGravity / 1_mps2;

        mFoundation.reset(PxCreateFoundation(PX_PHYSICS_VERSION, mAllocator, mErrorCallback));

        if (mFoundation == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX foundation.");

            return false;
        }

        ConnectPvd();

        mPhysics.reset(PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, mTolerancesScale, mPvd != nullptr, mPvd.get()));

        if (mPhysics == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX SDK instance.");
            Shutdown();

            return false;
        }

        mExtensionsInitialized = PxInitExtensions(*mPhysics, mPvd.get());

        if (!mExtensionsInitialized) {
            MACAW_LOG(LogPhysics, Error, "Cannot initialize the PhysX extensions.");
            Shutdown();

            return false;
        }

        mCooking.reset(PxCreateCooking(PX_PHYSICS_VERSION, *mFoundation, physx::PxCookingParams{mTolerancesScale}));

        if (mCooking == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX cooking interface.");
            Shutdown();

            return false;
        }

        mDefaultMaterial.reset(mPhysics->createMaterial(DefaultStaticFriction, DefaultDynamicFriction, DefaultRestitution));

        if (mDefaultMaterial == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the default PhysX material.");
            Shutdown();

            return false;
        }

        mDispatcher.reset(physx::PxDefaultCpuDispatcherCreate(DispatcherThreadCount));

        if (mDispatcher == nullptr) {
            MACAW_LOG(LogPhysics, Error, "Cannot create the PhysX CPU dispatcher.");
            Shutdown();

            return false;
        }

        MACAW_LOG(LogPhysics, Log, "PhysX initialized (length scale {}, speed scale {}, {} worker threads, PVD connected {}).", mTolerancesScale.length, mTolerancesScale.speed, DispatcherThreadCount, IsPvdConnected());

        return true;
    }

    /**
     * @brief 생성한 PhysX 객체를 의존 관계의 역순으로 해제합니다.
     */
    void PhysicsRuntime::FPhysicsRuntime::Shutdown() {
        mDispatcher.reset();
        mDefaultMaterial.reset();
        mCooking.reset();

        if (mExtensionsInitialized) {
            PxCloseExtensions();
            mExtensionsInitialized = false;
        }

        mPhysics.reset();
        mPvd.reset();
        mPvdTransport.reset();
        mFoundation.reset();
    }

    /**
     * @brief 필수 PhysX 객체가 모두 준비되었는지 반환합니다.
     */
    bool PhysicsRuntime::FPhysicsRuntime::IsInitialized() const {
        return mDispatcher != nullptr;
    }

    /**
     * @brief PVD에 연결되었는지 반환합니다.
     */
    bool PhysicsRuntime::FPhysicsRuntime::IsPvdConnected() const {
        return mPvd != nullptr && mPvd->isConnected();
    }

    /**
     * @brief SDK 객체와 쿠킹에 사용한 허용 오차 기준을 반환합니다.
     */
    const physx::PxTolerancesScale& PhysicsRuntime::FPhysicsRuntime::GetTolerancesScale() const {
        return mTolerancesScale;
    }

    /**
     * @brief PhysX SDK 객체의 비소유 주소를 반환합니다.
     */
    physx::PxPhysics* PhysicsRuntime::FPhysicsRuntime::GetPhysics() const {
        return mPhysics.get();
    }

    /**
     * @brief 충돌 메시 쿠킹 객체의 비소유 주소를 반환합니다.
     */
    physx::PxCooking* PhysicsRuntime::FPhysicsRuntime::GetCooking() const {
        return mCooking.get();
    }

    /**
     * @brief 별도 재질이 없는 형상에 사용할 기본 재질의 비소유 주소를 반환합니다.
     */
    physx::PxMaterial* PhysicsRuntime::FPhysicsRuntime::GetDefaultMaterial() const {
        return mDefaultMaterial.get();
    }

    /**
     * @brief 물리 장면이 공유하는 CPU 디스패처의 비소유 주소를 반환합니다.
     */
    physx::PxDefaultCpuDispatcher* PhysicsRuntime::FPhysicsRuntime::GetDispatcher() const {
        return mDispatcher.get();
    }

    /**
     * @brief PVD 객체의 비소유 주소를 반환합니다.
     */
    physx::PxPvd* PhysicsRuntime::FPhysicsRuntime::GetPvd() const {
        return mPvd.get();
    }

    /**
     * @brief PVD 객체를 만들고 로컬 PVD 프로그램에 연결을 시도합니다.
     */
    void PhysicsRuntime::FPhysicsRuntime::ConnectPvd() {
        mPvd.reset(physx::PxCreatePvd(*mFoundation));

        if (mPvd == nullptr) {
            MACAW_LOG(LogPhysics, Warning, "Cannot create the PhysX Visual Debugger instance.");

            return;
        }

        mPvdTransport.reset(physx::PxDefaultPvdSocketTransportCreate(PvdHost, PvdPort, PvdConnectTimeoutMilliseconds));

        if (mPvdTransport == nullptr) {
            MACAW_LOG(LogPhysics, Warning, "Cannot create the PhysX Visual Debugger transport.");

            return;
        }

        if (mPvd->connect(*mPvdTransport, physx::PxPvdInstrumentationFlag::eALL)) {
            MACAW_LOG(LogPhysics, Log, "Connected to the PhysX Visual Debugger at {}:{}.", PvdHost, PvdPort);
        } else {
            MACAW_LOG(LogPhysics, Log, "PhysX Visual Debugger is not running at {}:{}; continuing without it.", PvdHost, PvdPort);
        }
    }

}
