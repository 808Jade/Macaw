/**
 * @file UParticleSystemComponent.h
 * @brief 파티클 시스템 에셋을 월드에서 실행하는 컴포넌트입니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/Types.h"
#include "CoreUObject/TObjectRef.h"
#include "Engine/Component/UPrimitiveComponent.h"

#include <memory>

#include "UParticleSystemComponent.generated.h"

namespace Engine {

    class FParticleEmitterInstance;
    class FPrimitiveDrawInterface;
    class UParticleSystem;

    /**
     * @brief 템플릿 에셋의 에미터마다 인스턴스를 만들어 매 Tick 시뮬레이션합니다.
     * @details 템플릿은 에셋 캐시가 소유하며 읽기만 합니다. 에디터 월드에서도 Tick해 배치한 자리에서 미리 보입니다.
     */
    UCLASS()
    class UParticleSystemComponent final : public Engine::UPrimitiveComponent {
        GENERATED_BODY()

    public:
        UParticleSystemComponent();

        ~UParticleSystemComponent() override;

    public:
        const Core::FString& GetTemplatePath() const;

        /**
         * @brief 템플릿 에셋 경로를 바꿉니다. 등록된 상태면 바로 다시 로드하고 자동 활성화 설정에 따라 시작합니다.
         */
        bool SetTemplatePath(const Core::FString& Path);

        /**
         * @brief 로드한 템플릿이며 경로가 비었거나 로드에 실패하면 nullptr입니다.
         */
        Engine::UParticleSystem* GetTemplate() const;

        bool IsAutoActivate() const;

        void SetAutoActivate(bool AutoActivate);

        /**
         * @brief 인스턴스를 새로 만들고 LOD를 다시 적용합니다. 생성은 활성화 상태와 상관없이 켜진 상태로 시작합니다.
         */
        void InitializeSystem();

        /**
         * @brief 생성을 시작합니다. 인스턴스가 없거나 Reset이면 처음부터 다시 만듭니다.
         */
        void ActivateSystem(bool Reset = false);

        /**
         * @brief 생성만 멈춥니다. 살아 있는 파티클은 수명이 끝날 때까지 갱신됩니다.
         */
        void DeactivateSystem();

        /**
         * @brief 모든 인스턴스를 지웁니다.
         */
        void ResetParticles();

        /**
         * @brief 모든 에미터가 생성을 끝내고 살아 있는 파티클도 없으면 true입니다.
         */
        bool HasCompleted() const;

        const Core::TArray<std::unique_ptr<Engine::FParticleEmitterInstance>>& GetEmitterInstances() const;

        /**
         * @brief 인스턴스의 에미터 번호입니다.
         * @return 없으면 -1입니다.
         */
        Int32 GetEmitterIndex(const Engine::FParticleEmitterInstance* Instance) const;

        /**
         * @brief 현재 LOD 레벨입니다.
         */
        Int32 GetLODLevel() const;

        /**
         * @brief LOD 레벨을 직접 정합니다. DirectSet 방식에서 게임 코드가 씁니다.
         */
        void SetLODLevel(Int32 LODLevel);

        /**
         * @brief 에디터 미리보기에서 강제할 LOD이며 -1이면 끕니다. 게임 월드에서는 무시합니다.
         */
        Int32 GetPreviewLODOverride() const;

        void SetPreviewLODOverride(Int32 LODLevel);

        /**
         * @brief 살아 있는 파티클 수의 합입니다.
         */
        Int32 GetActiveParticleCount() const;

        /**
         * @brief 파티클 위치마다 작은 십자와 경계 상자를 그립니다.
         */
        void DrawDebug(Engine::FPrimitiveDrawInterface& DrawInterface, bool Selected, float Thickness) const;

        void Tick(float DeltaTime) override;

        void UnregisterComponent() override;

        Engine::EPrimitiveType GetPrimitiveType() const override;

        const Asset::FMeshData& GetMeshData() const override;

        FBox GetWorldBounds() const override;

        bool ShouldCreateRenderState() const override;

    private:
        void OnRegister() override;

        /**
         * @brief 경로의 템플릿을 로드합니다. 경로가 비었거나 실패하면 비웁니다.
         */
        void ResolveTemplate();

        /**
         * @brief 템플릿을 바꾼 뒤 인스턴스를 다시 만들고 필요하면 시작합니다.
         */
        void RestartForTemplate();

        /**
         * @brief 에셋의 LOD 방식에 따라 주기적으로 LOD를 다시 고릅니다.
         */
        void UpdateLODLevel(float DeltaTime);

        /**
         * @brief 범위를 맞춰 저장하고 모든 인스턴스에 전달합니다.
         */
        void ApplyLODLevel(Int32 LODLevel);

        /**
         * @brief 가장 가까운 뷰 거리로 고른 LOD이며 뷰를 알 수 없으면 -1입니다.
         */
        Int32 ComputeDesiredLOD() const;

        bool IsPreviewLODActive() const;

    private:
        UPROPERTY(Edit, Serialize, Category = "Particles", Name = "Template", Meta = (AssetType = "ParticleSystem", EmptyLabel = "None"), Getter = GetTemplatePath, Setter = SetTemplatePath)
        Core::FString mTemplatePath{};

        UPROPERTY(Edit, Serialize, Category = "Activation", Getter = IsAutoActivate, Setter = SetAutoActivate)
        bool mAutoActivate{true};

        CoreUObject::TObjectRef<Engine::UParticleSystem> mTemplate{};

        /**
         * @brief 인스턴스를 만든 템플릿입니다. 템플릿이 바뀌면 이 값과 달라집니다.
         */
        CoreUObject::TObjectRef<Engine::UParticleSystem> mInitializedTemplate{};

        Core::TArray<std::unique_ptr<Engine::FParticleEmitterInstance>> mEmitterInstances{};

        bool mWasCompleted{};

        Int32 mLODLevel{};

        float mLODCheckTimer{};

        Int32 mPreviewLODOverride{-1};

        /**
         * @brief 활성화할 때 뷰가 없어 LOD를 정하지 못했으면 true입니다.
         */
        bool mPendingActivateLOD{};
    };

}
