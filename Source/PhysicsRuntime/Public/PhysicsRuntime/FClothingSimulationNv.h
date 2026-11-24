/**
 * @file FClothingSimulationNv.h
 * @brief NvCloth 솔버로 천 입자를 시뮬레이션하는 클래스를 선언합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "Core/Math/FMatrix.h"
#include "Core/Math/FVector.h"

#include <NvCloth/PhaseConfig.h>
#include <foundation/PxVec4.h>

namespace nv::cloth {
    class Cloth;
    class ClothMeshQuadifier;
    class Fabric;
    class Factory;
    class Solver;
}

namespace PhysicsRuntime {
    class FClothingSystemRuntimeModuleNv;
    class UClothingAssetCommon;

    /**
     * @brief 천 에셋 하나로 만든 NvCloth 천과 그 상세 단계별 데이터입니다.
     * @ingroup PhysicsRuntime
     */
    struct FClothingActorNv {
        /**
         * @brief 천 상세 단계 하나에 대응하는 패브릭, 천, 제약 위상 정보입니다.
         */
        struct FActorLodData {
            /**
             * @brief 입자 사이 제약 구조를 담은 패브릭이며 참조 카운트로 해제합니다.
             */
            nv::cloth::Fabric* mFabric{};

            /**
             * @brief 입자 위치와 속도를 가진 천 인스턴스입니다.
             */
            nv::cloth::Cloth* mCloth{};

            /**
             * @brief 제약 위상마다 솔버가 쓸 세기 설정입니다.
             */
            Core::TArray<nv::cloth::PhaseConfig> mPhaseConfigs{};

            /**
             * @brief 제약 위상마다의 종류(늘어남, 굽힘, 전단 등)입니다.
             */
            Core::TArray<Int32> mPhaseTypes{};

            /**
             * @brief 천을 만들 때의 입자 위치와 역질량이며, 처음 상태로 되돌릴 때 씁니다.
             */
            Core::TArray<physx::PxVec4> mRestPositions{};
        };

        /**
         * @brief 천 상세 단계 순서의 데이터입니다.
         */
        Core::TArray<FActorLodData> mLodData{};

        /**
         * @brief 이 천을 만든 천 에셋이며 소유하지 않습니다. 코드로 만든 격자 천이면 nullptr입니다.
         */
        const UClothingAssetCommon* mAssetCreatedFrom{};

        /**
         * @brief 시뮬레이션 결과를 돌려줄 때 쓰는 번호입니다.
         */
        Index_t mSimDataIndex{InvalidIndex};

        /**
         * @brief 솔버에 들어가 있는 천 상세 단계 번호이며 시뮬레이션하지 않으면 InvalidIndex입니다.
         */
        Index_t mCurrentLodIndex{InvalidIndex};
    };

    /**
     * @brief 솔버 하나에 천들을 등록하고 프레임마다 시뮬레이션합니다.
     * @details 팩토리는 천 시스템 모듈이 소유하므로 이 객체는 모듈보다 먼저 정리되어야 합니다.
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
         * @brief 천 시스템 모듈의 팩토리로 솔버를 만듭니다.
         * @param[in] Module 팩토리와 사각형 변환 도구를 소유한 천 시스템 모듈입니다.
         * @return 솔버를 만들었거나 이미 초기화되었으면 true, 그렇지 않으면 false입니다.
         */
        bool Initialize(const FClothingSystemRuntimeModuleNv& Module);

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
         * @brief 천 에셋의 상세 단계마다 NvCloth 천을 만들고 현재 상세 단계의 천을 솔버에 넣습니다.
         * @details 입자는 현재 자세로 스키닝한 뒤 기준 뼈 좌표로 바꿔 넣고, 기준 뼈의 월드 변환을 천의 변환으로 둡니다.
         * @param[in] Asset 천 에셋이며 천보다 오래 살아 있어야 합니다.
         * @param[in] SimDataIndex 시뮬레이션 결과를 돌려줄 때 쓸 번호입니다.
         * @param[in] ComponentToWorld 컴포넌트의 월드 행렬입니다.
         * @param[in] ComponentSpaceTransforms 스켈레톤 뼈 순서의 컴포넌트 좌표 뼈 행렬입니다.
         * @param[in] RefToLocals 스켈레톤 뼈 순서의 스키닝 행렬(역바인드 * 컴포넌트 좌표 뼈 행렬)입니다.
         * @return 천을 하나 이상 만들었으면 true입니다.
         */
        bool CreateActor(const UClothingAssetCommon& Asset, Index_t SimDataIndex, const FMatrix& ComponentToWorld, const Core::TArray<FMatrix>& ComponentSpaceTransforms, const Core::TArray<FMatrix>& RefToLocals);

        /**
         * @brief 메시 상세 단계에 맞는 천 상세 단계를 골라 솔버의 천을 바꿉니다.
         * @details 새로 넣는 천은 입자를 현재 자세로 다시 놓고 관성, 충돌체, 모션 제약을 비웁니다.
         * @param[in] PredictedLod 메시의 상세 단계 번호입니다.
         * @param[in] ComponentToWorld 컴포넌트의 월드 행렬입니다.
         * @param[in] ComponentSpaceTransforms 스켈레톤 뼈 순서의 컴포넌트 좌표 뼈 행렬입니다.
         * @param[in] RefToLocals 스켈레톤 뼈 순서의 스키닝 행렬입니다.
         * @param[in] ForceNoRemap true이면 이전 상세 단계의 입자 위치를 옮기지 않습니다.
         * @param[in] ForceActorChecks true이면 상세 단계가 같아도 모든 천을 다시 검사합니다.
         */
        void UpdateLod(Index_t PredictedLod, const FMatrix& ComponentToWorld, const Core::TArray<FMatrix>& ComponentSpaceTransforms, const Core::TArray<FMatrix>& RefToLocals, bool ForceNoRemap, bool ForceActorChecks);

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
         * @details 에셋으로 만든 천은 기준 뼈 좌표이고, 격자 천은 월드 좌표입니다.
         * @param[in] ActorIndex 조회할 천의 순서입니다.
         * @param[out] OutPositions 입자 위치를 받을 배열이며 기존 내용은 지웁니다.
         * @return 시뮬레이션 중인 천이 있으면 true, 그렇지 않으면 false입니다.
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
         * @brief 패브릭을 굽기 전에 삼각형 메시를 사각형 메시로 바꾸는 도구이며 소유하지 않습니다.
         */
        nv::cloth::ClothMeshQuadifier* mMeshQuadifier{};

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
         * @brief 마지막으로 천을 맞춘 메시 상세 단계 번호입니다.
         */
        Index_t mCurrentMeshLodIndex{InvalidIndex};

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
