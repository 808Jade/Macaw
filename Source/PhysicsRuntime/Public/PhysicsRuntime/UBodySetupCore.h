/**
 * @file UBodySetupCore.h
 * @brief 충돌 모양 설계도의 공통 정보를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"
#include "CoreUObject/UObject.h"
#include "UBodySetupCore.generated.h"

namespace PhysicsRuntime {
    /**
     * @brief 충돌 모양 설계도의 공통 정보입니다.
     * @ingroup PhysicsRuntime
     */
    UCLASS()
    class UBodySetupCore : public CoreUObject::UObject {
        GENERATED_BODY()

    public:
        /**
         * @brief 뼈에 붙지 않은 빈 설계도를 구성합니다.
         */
        UBodySetupCore();

        /**
         * @brief 설계도를 정리합니다.
         */
        ~UBodySetupCore() override;

    public:
        /**
         * @brief 이 설계도가 붙을 뼈의 이름입니다. 뼈가 없는 메시는 비어 있습니다.
         */
        Core::FString mBoneName{};
    };
}
