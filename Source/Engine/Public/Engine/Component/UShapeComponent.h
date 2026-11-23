/**
 * @file UShapeComponent.h
 * @brief 렌더 프록시 없이 단순 충돌 형상을 제공하고 에디터 보조 선으로 표시하는 프리미티브 컴포넌트입니다.
 * @ingroup Engine
 */

#pragma once

#include "Engine/Component/UPrimitiveComponent.h"
#include "Engine/Collision/FCollisionShape.h"
#include "Core/Math/FVector4.h"
#include "Core/Math/FBox.h"
#include "UShapeComponent.generated.h"

namespace Engine {

    class FPrimitiveDrawInterface;

    /**
     * @brief 렌더 프록시 없이 단순 충돌 형상을 제공하고 에디터 보조 선으로 표시하는 프리미티브 컴포넌트입니다.
     */
    UCLASS(Abstract)
    class UShapeComponent : public Engine::UPrimitiveComponent {
        GENERATED_BODY()

    public:
        /**
         * @brief 전달된 설정과 기본값으로 UShapeComponent 객체를 생성합니다.
         */
        UShapeComponent();

    public:
        /**
         * @brief 검사에 사용할 충돌 형상 값을 반환합니다.
         * @return 계산하거나 조회한 값입니다.
         */
        virtual Engine::FCollisionShape GetCollisionShape() const = 0;

        /**
         * @brief 충돌 형상의 로컬 축 정렬 경계를 계산합니다.
         * @return 계산하거나 조회한 값입니다.
         */
        virtual FBox CalcLocalBounds() const = 0;

        /**
         * @brief 컴포넌트 변환을 적용한 월드 충돌 경계를 계산합니다.
         * @return 계산하거나 조회한 값입니다.
         */
        FBox CalcBounds() const;

        /**
         * @brief 월드 공간의 축 정렬 경계 상자를 반환합니다.
         * @return 월드 공간의 축 정렬 경계 상자입니다.
         */
        FBox GetWorldBounds() const override;

        /**
         * @brief 표시할 프리미티브 메시 종류를 반환합니다.
         * @return 표시할 프리미티브 메시 종류입니다.
         */
        Engine::EPrimitiveType GetPrimitiveType() const override;

        /**
         * @brief 충돌 형상을 선택 강조 색상 또는 기본 색상의 선으로 수집합니다.
         * @param[in] DrawInterface 보조 선분을 추가할 프레임 수집기입니다.
         * @param[in] Highlight 선택 강조 색상을 사용할지 지정합니다.
         */
        void DrawDebug(Engine::FPrimitiveDrawInterface& DrawInterface, bool Highlight) const;

        /**
         * @brief 두 컴포넌트의 월드 충돌 형상이 겹치는지 검사합니다.
         * @param[in] Other 비교할 상대 값입니다.
         * @return 조건을 만족하면 true입니다.
         */
        bool IsOverlapping(const Engine::UShapeComponent& Other) const;

        /**
         * @brief 두 형상의 월드 접촉 법선·깊이·위치를 계산합니다.
         * @param[in] Other 비교할 상대 값입니다.
         * @param[out] OutContact 접촉 정보를 기록할 선택적 출력 주소입니다.
         * @return 두 형상이 겹쳐 접촉 정보를 계산했으면 true입니다.
         */
        bool ComputeContact(const Engine::UShapeComponent& Other, Engine::FCollisionContact& OutContact) const;

        /**
         * @brief 충돌 도형을 표시할 RGBA 색상을 반환합니다.
         * @return 이 객체가 보관하는 값의 참조입니다.
         */
        const FVector4& GetShapeColor() const;

        /**
         * @brief 충돌 도형을 표시할 RGBA 색상을 설정합니다.
         * @param[in] Color 적용할 색상입니다.
         * @return 값이 유효해 적용했으면 true, 거부했으면 false입니다.
         */
        bool SetShapeColor(const FVector4& Color);

        /**
         * @brief 선택한 경우에만 보조 충돌 도형을 그릴지 검사합니다.
         * @return 조건을 만족하면 true입니다.
         */
        bool ShouldDrawOnlyIfSelected() const;

        /**
         * @brief 선택한 경우에만 충돌 도형을 표시할지 여부를 설정합니다.
         * @param[in] DrawOnlyIfSelected 선택할 때만 보조 도형을 표시할지 지정합니다.
         */
        void SetDrawOnlyIfSelected(bool DrawOnlyIfSelected);

        /**
         * @brief 충돌 형상에 적용할 최대 절대 축 스케일을 반환합니다.
         * @return 계산하거나 조회한 값입니다.
         */
        float GetShapeScale() const;

    private:
        /**
         * @brief 현재 표시·에셋 상태에서 렌더 프록시를 생성할지 판정합니다.
         * @return 조건을 만족하면 true입니다.
         */
        bool ShouldCreateRenderState() const override;

    private:
        /**
         * @brief 충돌 도형을 표시할 RGBA 색상입니다.
         */
        UPROPERTY(Edit, Serialize, Getter = GetShapeColor, Setter = SetShapeColor)
        FVector4 mShapeColor{223.0f / 255.0f, 149.0f / 255.0f, 157.0f / 255.0f, 1.0f};

        /**
         * @brief 선택한 경우에만 충돌 도형을 표시할지 여부입니다.
         */
        UPROPERTY(Edit, Serialize, Getter = ShouldDrawOnlyIfSelected, Setter = SetDrawOnlyIfSelected)
        bool mDrawOnlyIfSelected{};
    };

}
