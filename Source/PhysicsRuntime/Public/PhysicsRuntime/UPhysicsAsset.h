/**
 * @file UPhysicsAsset.h
 * @brief 스켈레탈 메시의 뼈별 충돌 모양과 관절을 묶은 물리 에셋을 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "CoreUObject/UObject.h"
#include "PhysicsRuntime/UBodySetup.h"
#include "PhysicsRuntime/UPhysicsConstraintTemplate.h"

#include <memory>

#include "UPhysicsAsset.generated.h"

namespace PhysicsRuntime {
    /**
     * @brief 스켈레탈 메시의 뼈별 충돌 모양과 관절을 묶은 물리 에셋입니다.
     * @ingroup PhysicsRuntime
     * @note 뼈는 이름으로만 가리키므로 스켈레톤 타입을 알지 않습니다.
     */
    UCLASS()
    class UPhysicsAsset : public CoreUObject::UObject {
        GENERATED_BODY()

    public:
        /**
         * @brief 바디와 관절이 없는 빈 에셋을 구성합니다.
         */
        UPhysicsAsset();

        /**
         * @brief 소유한 설계도와 관절 설정을 정리합니다.
         */
        ~UPhysicsAsset() override;

    public:
        /**
         * @brief 뼈 이름으로 바디 설계도의 번호를 찾습니다.
         * @param[in] BodyName 찾을 뼈의 이름입니다.
         * @return 설계도 번호이며, 없으면 InvalidIndex입니다.
         */
        Index_t FindBodyIndex(const Core::FString& BodyName) const;

        /**
         * @brief 관절 이름으로 관절 설정의 번호를 찾습니다.
         * @param[in] ConstraintName 찾을 관절의 이름입니다.
         * @return 관절 번호이며, 없으면 InvalidIndex입니다.
         */
        Index_t FindConstraintIndex(const Core::FString& ConstraintName) const;

        /**
         * @brief 뼈 이름에서 설계도 번호를 찾는 표를 다시 만듭니다.
         * @note 설계도를 추가하거나 지운 뒤 호출합니다.
         */
        void UpdateBodySetupIndexMap();

    public:
        /**
         * @brief 뼈마다 하나씩 가진 충돌 모양 설계도입니다.
         */
        Core::TArray<std::unique_ptr<UBodySetup>> mSkeletalBodySetups{};

        /**
         * @brief 두 뼈를 잇는 관절 설정입니다.
         */
        Core::TArray<std::unique_ptr<UPhysicsConstraintTemplate>> mConstraintSetup{};

    private:
        /**
         * @brief 뼈 이름에서 설계도 번호를 찾는 표입니다.
         */
        Core::TMap<Core::FString, Index_t> mBodySetupIndexMap{};
    };
}
