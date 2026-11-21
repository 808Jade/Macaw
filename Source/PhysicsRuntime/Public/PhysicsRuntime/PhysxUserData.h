/**
 * @file PhysxUserData.h
 * @brief PhysX 객체의 userData에 종류와 주소를 함께 담는 꼬리표를 정의합니다.
 * @ingroup PhysicsRuntime
 */

#pragma once

#include "Core/Types.h"

namespace PhysicsRuntime {
    class FBodyInstance;
    class FPhysicsScene;

    /**
     * @brief userData가 가리키는 객체의 종류입니다.
     * @ingroup PhysicsRuntime
     */
    enum class EPhysxUserDataType : Uint8 {
        /**
         * @brief 아무것도 가리키지 않습니다.
         */
        Invalid,

        /**
         * @brief FBodyInstance를 가리킵니다.
         */
        BodyInstance,

        /**
         * @brief FPhysicsScene을 가리킵니다.
         */
        PhysScene,

        /**
         * @brief 유효한 종류의 개수이며 손상 검사에 사용합니다.
         */
        Count
    };

    /**
     * @brief PhysX 객체의 userData에 넣는 종류와 주소의 묶음입니다.
     * @ingroup PhysicsRuntime
     * @note 꼬리표는 가리키는 객체의 멤버로 두어 수명을 같게 합니다.
     */
    struct FPhysxUserData {
    public:
        /**
         * @brief 아무것도 가리키지 않는 꼬리표를 구성합니다.
         */
        FPhysxUserData() = default;

        /**
         * @brief 바디를 가리키는 꼬리표를 구성합니다.
         * @param[in] Payload 가리킬 바디입니다.
         */
        explicit FPhysxUserData(FBodyInstance* Payload)
            : mType{EPhysxUserDataType::BodyInstance}, mPayload{Payload} {}

        /**
         * @brief 물리 장면을 가리키는 꼬리표를 구성합니다.
         * @param[in] Payload 가리킬 물리 장면입니다.
         */
        explicit FPhysxUserData(FPhysicsScene* Payload)
            : mType{EPhysxUserDataType::PhysScene}, mPayload{Payload} {}

    public:
        /**
         * @brief userData에서 요청한 종류의 객체를 꺼냅니다.
         * @tparam T 꺼낼 객체의 타입입니다.
         * @param[in] UserData PhysX 객체의 userData입니다.
         * @return 종류가 일치하면 객체의 비소유 주소, 비어 있거나 다르면 nullptr입니다.
         */
        template <class T>
        static T* Get(void* UserData);

        /**
         * @brief userData의 종류 값이 범위를 벗어났는지 검사합니다.
         * @param[in] UserData 검사할 userData이며 nullptr이 아니어야 합니다.
         * @return 종류 값이 손상되었으면 true입니다.
         * @note 이미 해제된 객체를 가리키는 userData를 찾을 때 사용합니다.
         */
        static bool IsGarbage(void* UserData) {
            return static_cast<FPhysxUserData*>(UserData)->mType >= EPhysxUserDataType::Count;
        }

    private:
        /**
         * @brief 종류가 일치할 때만 주소를 반환합니다.
         * @param[in] UserData PhysX 객체의 userData입니다.
         * @param[in] Type 요청한 종류입니다.
         * @return 종류가 일치하면 주소, 아니면 nullptr입니다.
         */
        static void* GetPayload(void* UserData, EPhysxUserDataType Type) {
            const auto* Data{static_cast<FPhysxUserData*>(UserData)};

            return Data != nullptr && Data->mType == Type ? Data->mPayload : nullptr;
        }

    private:
        /**
         * @brief 가리키는 객체의 종류입니다.
         */
        EPhysxUserDataType mType{EPhysxUserDataType::Invalid};

        /**
         * @brief 가리키는 객체의 비소유 주소입니다.
         */
        void* mPayload{};
    };

    /**
     * @brief userData에서 바디를 꺼냅니다.
     * @param[in] UserData PhysX 객체의 userData입니다.
     * @return 바디를 가리키면 그 주소, 아니면 nullptr입니다.
     */
    template <>
    inline FBodyInstance* FPhysxUserData::Get<FBodyInstance>(void* UserData) {
        return static_cast<FBodyInstance*>(GetPayload(UserData, EPhysxUserDataType::BodyInstance));
    }

    /**
     * @brief userData에서 물리 장면을 꺼냅니다.
     * @param[in] UserData PhysX 객체의 userData입니다.
     * @return 물리 장면을 가리키면 그 주소, 아니면 nullptr입니다.
     */
    template <>
    inline FPhysicsScene* FPhysxUserData::Get<FPhysicsScene>(void* UserData) {
        return static_cast<FPhysicsScene*>(GetPayload(UserData, EPhysxUserDataType::PhysScene));
    }
}
