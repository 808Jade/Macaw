/**
 * @file FClothingSimulationNv.h
 * @brief NvCloth 솔버로 천 입자를 시뮬레이션하는 클래스를 선언합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "Core/Math/FVector.h"

namespace nv::cloth {
    class Cloth;
    class Fabric;
    class Factory;
    class Solver;
}

namespace PhysicsRuntime {

    /**
     * @brief 솔버에 등록한 천 하나와 그 천이 공유하는 패브릭입니다.
     * @ingroup PhysicsRuntime
     */
    struct FClothingActorNv {
        /**
         * @brief 입자 사이 제약 구조를 담은 패브릭이며 참조 카운트로 해제합니다.
         */
        nv::cloth::Fabric* mFabric{};

        /**
         * @brief 입자 위치와 속도를 가진 천 인스턴스입니다.
         */
        nv::cloth::Cloth* mCloth{};
    };

    /**
     * @brief 솔버 하나에 천들을 등록하고 프레임마다 시뮬레이션합니다.
     * @details 팩토리는 천 시스템 모듈이 소유하므로 이 객체는 팩토리보다 먼저 정리되어야 합니다.
     * @ingroup PhysicsRuntime
     */
    class FClothingSimulationNv {
    public:

        /**
         * @brief 솔버가 없는 빈 시뮬레이션을 구성합니다.
         */
        FClothingSimulationNv();

        /**
         * @brief 등록한 천과 솔버를 해제합니다.
         */
        ~FClothingSimulationNv();

        /**
         * @brief 솔버와 천을 두 객체가 해제하지 않도록 복사를 금지합니다.
         */
        FClothingSimulationNv(const FClothingSimulationNv&) = delete;

        /**
         * @brief 솔버와 천을 두 객체가 해제하지 않도록 대입을 금지합니다.
         */
        FClothingSimulationNv& operator=(const FClothingSimulationNv&) = delete;

    public:

        /**
         * @brief 팩토리로 솔버를 만듭니다.
         * @param[in] Factory 천 시스템 모듈이 소유한 CPU 팩토리입니다.
         * @return 솔버를 만들었거나 이미 초기화되었으면 true, 그렇지 않으면 false입니다.
         */
        bool Initialize(nv::cloth::Factory* Factory);

        /**
         * @brief 등록한 천을 모두 해제한 뒤 솔버를 해제합니다.
         * @note 여러 번 호출해도 안전합니다.
         */
        void Shutdown();

        /**
         * @brief 한 변을 고정한 격자 천을 만들어 솔버에 등록합니다.
         * @details 격자는 XY 평면에 눕혀 만들고 첫 줄 입자의 역질량을 0으로 두어 고정합니다.
         *          메시 에셋 없이 솔버 동작을 확인하기 위한 임시 함수입니다.
         * @param[in] Origin 첫 입자의 월드 위치입니다.
         * @param[in] Width X 방향 입자 수이며 2 이상이어야 합니다.
         * @param[in] Height Y 방향 입자 수이며 2 이상이어야 합니다.
         * @param[in] Spacing 이웃 입자 사이의 거리(미터)입니다.
         * @return 천을 만들어 등록했으면 true, 그렇지 않으면 false입니다.
         */
        bool CreateGridActor(const FVector& Origin, Uint32 Width, Uint32 Height, float Spacing);

        /**
         * @brief 등록한 천을 솔버에서 빼고 천과 패브릭을 해제합니다.
         */
        void DestroyActors();

        /**
         * @brief 모든 천에 중력을 적용하고 솔버를 한 프레임 진행합니다.
         * @param[in] DeltaTime 진행할 초 단위 시간이며 너무 크면 잘라 냅니다.
         */
        void Simulate(float DeltaTime);

        /**
         * @brief 천 하나의 현재 입자 위치를 복사합니다.
         * @param[in] ActorIndex 조회할 천의 순서입니다.
         * @param[out] OutPositions 입자 위치를 받을 배열이며 기존 내용은 지웁니다.
         * @return 천이 있으면 true, 그렇지 않으면 false입니다.
         */
        bool GetParticles(Index_t ActorIndex, Core::TArray<FVector>& OutPositions) const;

        /**
         * @brief 등록한 천의 개수를 반환합니다.
         * @return 솔버에 등록한 천의 개수입니다.
         */
        Index_t GetNumActors() const;

        /**
         * @brief 마지막으로 만든 격자의 X 방향 입자 수를 반환합니다.
         * @return 격자가 없으면 0입니다.
         */
        Uint32 GetGridWidth() const;

        /**
         * @brief 마지막으로 만든 격자의 Y 방향 입자 수를 반환합니다.
         * @return 격자가 없으면 0입니다.
         */
        Uint32 GetGridHeight() const;

    private:

        /**
         * @brief 천, 패브릭, 솔버를 만드는 팩토리이며 소유하지 않습니다.
         */
        nv::cloth::Factory* mFactory{};

        /**
         * @brief 등록한 천을 함께 진행하는 솔버입니다.
         */
        nv::cloth::Solver* mSolver{};

        /**
         * @brief 솔버에 등록한 천 목록입니다.
         */
        Core::TArray<FClothingActorNv> mActors{};

        /**
         * @brief 모든 천에 적용할 중력 가속도(m/s²)입니다.
         */
        FVector mGravity{0.0f, 0.0f, -9.81f};

        /**
         * @brief 마지막으로 만든 격자의 X 방향 입자 수입니다.
         */
        Uint32 mGridWidth{};

        /**
         * @brief 마지막으로 만든 격자의 Y 방향 입자 수입니다.
         */
        Uint32 mGridHeight{};
    };
}
