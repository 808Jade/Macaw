/**
 * @file FParticleRenderData.h
 * @brief 파티클 컴포넌트가 매 Tick 렌더러에 넘기는 에미터별 그리기 데이터입니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/Types.h"
#include "CoreUObject/TObjectRef.h"
#include "Engine/Particles/Module/UParticleModuleRequired.h"

namespace Asset {
    class UMaterial;
}

namespace Engine {

    /**
     * @brief 스프라이트 파티클 하나의 GPU 인스턴스입니다. 위치와 크기는 월드 공간(m)입니다.
     * @details ParticleSprite.hlsl의 FParticleSpriteInstance와 배치가 같아야 합니다.
     */
    struct FParticleSpriteInstance {
    public:
        float mPosition[3]{};

        /**
         * @brief 화면 평면 안의 회전입니다(rad).
         */
        float mRotation{};

        float mSize[2]{};

        /**
         * @brief 정수부는 SubUV 칸 번호, 소수부는 다음 칸과 섞는 비율입니다.
         */
        float mSubImageIndex{};

        /**
         * @brief 생성 순번이며 나이순 정렬에 씁니다.
         */
        Uint32 mCounter{};

        float mColor[4]{};
    };

    static_assert(sizeof(Engine::FParticleSpriteInstance) == 48, "FParticleSpriteInstance must match ParticleSprite.hlsl");

    /**
     * @brief 에미터 하나의 스프라이트 묶음입니다. 정렬은 뷰마다 렌더러가 합니다.
     */
    struct FParticleSpriteBatch {
    public:
        Core::TArray<Engine::FParticleSpriteInstance> mInstances{};

        /**
         * @brief 기본 색 텍스처와 색조를 쓰며 비어 있으면 기본 원형 텍스처를 씁니다.
         */
        CoreUObject::TObjectRef<Asset::UMaterial> mMaterial{};

        Engine::EParticleSortMode mSortMode{Engine::EParticleSortMode::None};

        Uint32 mSubImagesHorizontal{1};

        Uint32 mSubImagesVertical{1};
    };

    /**
     * @brief 컴포넌트 하나의 그리기 데이터입니다. 배열 순서가 에미터 그리기 순서입니다.
     */
    struct FParticleRenderData {
    public:
        Core::TArray<Engine::FParticleSpriteBatch> mSprites{};
    };

}
