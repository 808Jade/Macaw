#include "Engine/Component/UParticleSystemComponent.h"
#include "Engine/Component/UCameraComponent.h"
#include "Engine/Particles/FBaseParticle.h"
#include "Engine/Particles/FParticleEmitterInstance.h"
#include "Engine/Particles/ParticleAssets.h"
#include "Engine/Particles/UParticleEmitter.h"
#include "Engine/Particles/UParticleSystem.h"
#include "Engine/FPrimitiveDrawInterface.h"
#include "Engine/UWorld.h"
#include "Asset/FAssetManager.h"
#include "Asset/FMeshData.h"
#include "Core/FilePath.h"
#include "Core/Log/Log.h"

#include <algorithm>
#include <cfloat>

DEFINE_LOG_CATEGORY_STATIC(LogParticles, Log, All);

namespace Engine {

    namespace {

        /**
         * @brief 디버그 십자의 반 길이(m)입니다.
         */
        constexpr float DebugCrossHalfLength{0.03f};

        /**
         * @brief 파티클이 없을 때 쓰는 경계 상자의 반 크기(m)입니다.
         */
        constexpr float EmptyBoundsHalfExtent{0.1f};

    }

    Engine::UParticleSystemComponent::UParticleSystemComponent() {
        // 에디터 월드에서도 배치한 자리에서 미리 보이도록 Tick합니다.
        SetEditorTickable(true);
    }

    Engine::UParticleSystemComponent::~UParticleSystemComponent() = default;

    const Core::FString& Engine::UParticleSystemComponent::GetTemplatePath() const {
        return mTemplatePath;
    }

    bool Engine::UParticleSystemComponent::SetTemplatePath(const Core::FString& Path) {
        if (Path == mTemplatePath) {
            return true;
        }

        mTemplatePath = Path;

        if (IsRegistered()) {
            ResolveTemplate();
            RestartForTemplate();
        }

        return true;
    }

    Engine::UParticleSystem* Engine::UParticleSystemComponent::GetTemplate() const {
        return mTemplate.Get();
    }

    bool Engine::UParticleSystemComponent::IsAutoActivate() const {
        return mAutoActivate;
    }

    void Engine::UParticleSystemComponent::SetAutoActivate(bool AutoActivate) {
        mAutoActivate = AutoActivate;
    }

    void Engine::UParticleSystemComponent::InitializeSystem() {
        ResetParticles();
        mInitializedTemplate = mTemplate;

        if (mTemplate == nullptr) {
            return;
        }

        for (const auto& Emitter : mTemplate->GetEmitters()) {
            if (auto Instance{Emitter->CreateInstance(*this)}) {
                mEmitterInstances.push_back(std::move(Instance));
            }
        }

        mWasCompleted = false;

        // 새 인스턴스는 LOD 0으로 시작하므로 지금 LOD를 다시 적용합니다.
        ApplyLODLevel(IsPreviewLODActive() ? mPreviewLODOverride : mLODLevel);
    }

    void Engine::UParticleSystemComponent::ActivateSystem(bool Reset) {
        // 인스턴스가 다른 템플릿으로 만들어졌으면 다시 만듭니다.
        if (mEmitterInstances.empty() || Reset || mInitializedTemplate.Get() != mTemplate.Get()) {
            InitializeSystem();
        }

        for (const auto& Instance : mEmitterInstances) {
            Instance->mSpawningEnabled = true;
        }

        mWasCompleted = false;
        mLODCheckTimer = 0.0f;
        mPendingActivateLOD = false;

        // 멀리서 터진 이펙트가 LOD 0 Burst로 시작하지 않도록 첫 Tick 전에 거리로 정합니다.
        if (mTemplate != nullptr && !IsPreviewLODActive() && mTemplate->GetLODMethod() != Engine::EParticleSystemLODMethod::DirectSet) {
            const Int32 Desired{ComputeDesiredLOD()};

            if (Desired >= 0) {
                ApplyLODLevel(Desired);
            } else {
                // 아직 뷰가 없으면 뷰가 생기는 첫 Tick에서 정합니다.
                mPendingActivateLOD = true;
            }
        }
    }

    void Engine::UParticleSystemComponent::DeactivateSystem() {
        for (const auto& Instance : mEmitterInstances) {
            Instance->mSpawningEnabled = false;
        }
    }

    void Engine::UParticleSystemComponent::ResetParticles() {
        mEmitterInstances.clear();
    }

    bool Engine::UParticleSystemComponent::HasCompleted() const {
        return mWasCompleted;
    }

    const Core::TArray<std::unique_ptr<Engine::FParticleEmitterInstance>>& Engine::UParticleSystemComponent::GetEmitterInstances() const {
        return mEmitterInstances;
    }

    Int32 Engine::UParticleSystemComponent::GetEmitterIndex(const Engine::FParticleEmitterInstance* Instance) const {
        for (Index_t Index{}; Index < mEmitterInstances.size(); ++Index) {
            if (mEmitterInstances[Index].get() == Instance) {
                return static_cast<Int32>(Index);
            }
        }

        return -1;
    }

    Int32 Engine::UParticleSystemComponent::GetLODLevel() const {
        return mLODLevel;
    }

    void Engine::UParticleSystemComponent::SetLODLevel(Int32 LODLevel) {
        ApplyLODLevel(LODLevel);
    }

    Int32 Engine::UParticleSystemComponent::GetPreviewLODOverride() const {
        return mPreviewLODOverride;
    }

    void Engine::UParticleSystemComponent::SetPreviewLODOverride(Int32 LODLevel) {
        mPreviewLODOverride = LODLevel < 0 ? -1 : LODLevel;

        if (IsPreviewLODActive()) {
            ApplyLODLevel(mPreviewLODOverride);
        } else if (mTemplate != nullptr) {
            // 미리보기를 끄면 다음 Tick에서 바로 거리로 다시 고르게 합니다.
            mLODCheckTimer = mTemplate->GetLODDistanceCheckTime().GetBase();
        }
    }

    Int32 Engine::UParticleSystemComponent::GetActiveParticleCount() const {
        Int32 Count{};

        for (const auto& Instance : mEmitterInstances) {
            Count += Instance->mActiveParticles;
        }

        return Count;
    }

    void Engine::UParticleSystemComponent::DrawDebug(Engine::FPrimitiveDrawInterface& DrawInterface, bool Selected, float Thickness) const {
        const FVector Color{1.0f, 1.0f, 0.0f};

        for (const auto& Instance : mEmitterInstances) {
            for (Int32 Index{}; Index < Instance->mActiveParticles; ++Index) {
                const FVector Location{Instance->SimulationToWorld(Instance->GetParticle(Instance->mParticleIndices[Index])->mLocation)};

                DrawInterface.DrawLine(Location - FVector{DebugCrossHalfLength, 0.0f, 0.0f}, Location + FVector{DebugCrossHalfLength, 0.0f, 0.0f}, Color, true, Thickness);
                DrawInterface.DrawLine(Location - FVector{0.0f, DebugCrossHalfLength, 0.0f}, Location + FVector{0.0f, DebugCrossHalfLength, 0.0f}, Color, true, Thickness);
                DrawInterface.DrawLine(Location - FVector{0.0f, 0.0f, DebugCrossHalfLength}, Location + FVector{0.0f, 0.0f, DebugCrossHalfLength}, Color, true, Thickness);
            }
        }

        if (Selected) {
            DrawInterface.DrawBox(GetWorldBounds(), {1.0f, 0.65f, 0.15f}, true, Thickness);
        }
    }

    void Engine::UParticleSystemComponent::Tick(float DeltaTime) {
        Engine::UPrimitiveComponent::Tick(DeltaTime);

        // 경로만 바뀌고 인스턴스가 옛 템플릿으로 남은 경우(복제·로드 직후)를 맞춥니다.
        if (mInitializedTemplate.Get() != mTemplate.Get()) {
            RestartForTemplate();
        }

        if (mEmitterInstances.empty()) {
            return;
        }

        // 인스턴스가 이번 Tick을 새 LOD의 모듈로 돌도록 먼저 고릅니다.
        UpdateLODLevel(DeltaTime);

        bool AllComplete{true};

        for (const auto& Instance : mEmitterInstances) {
            Instance->Tick(DeltaTime);
            AllComplete = AllComplete && Instance->IsComplete();
        }

        mWasCompleted = AllComplete;
    }

    void Engine::UParticleSystemComponent::UnregisterComponent() {
        ResetParticles();
        Engine::UPrimitiveComponent::UnregisterComponent();
    }

    Engine::EPrimitiveType Engine::UParticleSystemComponent::GetPrimitiveType() const {
        return Engine::EPrimitiveType::Plane;
    }

    const Asset::FMeshData& Engine::UParticleSystemComponent::GetMeshData() const {
        static const Asset::FMeshData Empty{};

        return Empty;
    }

    FBox Engine::UParticleSystemComponent::GetWorldBounds() const {
        // 인스턴스가 자기 공간(Local/World)에 맞춰 월드 상자를 만들어 두므로 합치기만 합니다.
        FVector Minimum{FLT_MAX, FLT_MAX, FLT_MAX};
        FVector Maximum{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        bool Any{};

        for (const auto& Instance : mEmitterInstances) {
            if (Instance->mActiveParticles == 0) {
                continue;
            }

            Minimum = Minimum.Min(Instance->mBoundingBox.mMinimum);
            Maximum = Maximum.Max(Instance->mBoundingBox.mMaximum);
            Any = true;
        }

        if (!Any) {
            const FVector Location{GetWorldMatrix().TransformPosition({})};
            const FVector Extent{EmptyBoundsHalfExtent, EmptyBoundsHalfExtent, EmptyBoundsHalfExtent};

            return FBox{Location - Extent, Location + Extent};
        }

        return FBox{Minimum, Maximum};
    }

    bool Engine::UParticleSystemComponent::ShouldCreateRenderState() const {
        // 파티클 렌더러가 생기기 전까지는 렌더 상태를 만들지 않습니다.
        return false;
    }

    void Engine::UParticleSystemComponent::OnRegister() {
        ResolveTemplate();
        RestartForTemplate();
    }

    void Engine::UParticleSystemComponent::ResolveTemplate() {
        mTemplate.Reset();

        Asset::FAssetManager* Assets{GetWorld() != nullptr ? GetWorld()->GetAssets() : nullptr};

        if (mTemplatePath.empty() || Assets == nullptr) {
            return;
        }

        Core::FString Error{};

        mTemplate = Engine::LoadParticleSystem(*Assets, Core::FilePath::FromUtf8(mTemplatePath), Error);

        if (mTemplate == nullptr) {
            MACAW_LOG(LogParticles, Warning, "Cannot load particle system '{}': {}", mTemplatePath, Error);
        }
    }

    void Engine::UParticleSystemComponent::RestartForTemplate() {
        if (mAutoActivate) {
            ActivateSystem(true);
        } else {
            InitializeSystem();
            DeactivateSystem();
        }
    }

    void Engine::UParticleSystemComponent::UpdateLODLevel(float DeltaTime) {
        if (mTemplate == nullptr || IsPreviewLODActive()) {
            return;
        }

        if (mPendingActivateLOD) {
            const Int32 Desired{ComputeDesiredLOD()};

            if (Desired >= 0) {
                ApplyLODLevel(Desired);
                mPendingActivateLOD = false;
                mLODCheckTimer = 0.0f;
            }

            return;
        }

        // DirectSet은 코드가 정한 값, ActivateAutomatic은 활성화 때 정한 값을 유지합니다.
        if (mTemplate->GetLODMethod() != Engine::EParticleSystemLODMethod::Automatic) {
            return;
        }

        mLODCheckTimer += DeltaTime;

        if (mLODCheckTimer < mTemplate->GetLODDistanceCheckTime().GetBase()) {
            return;
        }

        mLODCheckTimer = 0.0f;

        const Int32 Desired{ComputeDesiredLOD()};

        if (Desired >= 0 && Desired != mLODLevel) {
            ApplyLODLevel(Desired);
        }
    }

    void Engine::UParticleSystemComponent::ApplyLODLevel(Int32 LODLevel) {
        const Int32 Count{mTemplate != nullptr ? static_cast<Int32>(mTemplate->GetNumLODs()) : 1};

        mLODLevel = std::clamp(LODLevel, 0, std::max(Count - 1, 0));

        for (const auto& Instance : mEmitterInstances) {
            Instance->SetCurrentLODIndex(mLODLevel);
        }
    }

    Int32 Engine::UParticleSystemComponent::ComputeDesiredLOD() const {
        const Engine::UWorld* World{GetWorld()};
        const Engine::UCameraComponent* Camera{World != nullptr ? World->GetActiveCamera() : nullptr};

        // 지금은 월드의 활성 카메라만 뷰로 봅니다. 활성 카메라가 없는 편집 월드는 LOD를 바꾸지 않습니다.
        if (mTemplate == nullptr || Camera == nullptr) {
            return -1;
        }

        const FVector ViewLocation{Camera->GetWorldMatrix().TransformPosition({})};
        const float DistanceSquared{ViewLocation.DistanceSquared(GetWorldMatrix().TransformPosition({}))};
        const auto& Distances{mTemplate->GetLODDistances()};

        for (Index_t Index{Distances.size()}; Index-- > 1;) {
            if (DistanceSquared >= Distances[Index] * Distances[Index]) {
                return static_cast<Int32>(Index);
            }
        }

        return 0;
    }

    bool Engine::UParticleSystemComponent::IsPreviewLODActive() const {
        const Engine::UWorld* World{GetWorld()};

        return mPreviewLODOverride >= 0 && (World == nullptr || World->GetWorldType() == Engine::EWorldType::Editor || World->GetWorldType() == Engine::EWorldType::EditorPreview);
    }

}
