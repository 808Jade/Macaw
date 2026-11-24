#pragma once

#include "FAssetEditorToolkit.h"
#include "Editor/FAssetPicker.h"
#include "Engine/Curves/FFloatCurveKey.h"
#include "Viewer/FAssetViewer.h"

namespace Asset {
    class FAssetManager;
}

namespace CoreUObject {
    class FProperty;
    class UClass;
    class UObject;
}

namespace Engine {
    class UParticleEmitter;
    class UParticleLODLevel;
    class UParticleModule;
    class UParticleSystem;
}

namespace Editor {

    class FEditorAssetCatalog;
    class FAssetThumbnailCache;
    class FEditorContext;
    class FParticleEditorPreview;

    class FParticleEditorToolkit final : public Editor::FAssetEditorToolkit {
    private:
        enum class ESelection {
            System,
            Emitter,
            Required,
            Spawn,
            TypeData,
            Module
        };

    public:
        FParticleEditorToolkit(Editor::FEditorContext& Context, const std::filesystem::path& Path, Editor::FEditorAssetCatalog& Catalog, Editor::FAssetThumbnailCache& Thumbnails, Editor::FMessageBox& Messages);

        ~FParticleEditorToolkit() override;

    public:
        bool Open(Engine::UParticleSystem& Asset, Core::FString& Error);
        void CloseRetired(const std::filesystem::path& Absolute) override;
        std::optional<Editor::FMessageBox::FSavePrompt> GetClosePrompt() override;

    private:
        void UpdateEditor(float DeltaTime) override;
        void HandleCommands() override;
        bool RequestClose();
        bool Save();
        bool Reload(Core::FString& Error);
        void UpdateTitle();
        void RestartPreview();
        void MarkDirty();
        void ResetCurveSelection();
        void Select(Editor::FParticleEditorToolkit::ESelection Selection, Index_t Module = 0);
        Engine::UParticleEmitter* GetEmitter() const;
        Engine::UParticleLODLevel* GetLevel() const;
        CoreUObject::UObject* GetSelectedObject() const;

        void DrawPreview();
        void DrawEmitters();
        void DrawLODControls();
        void DrawModuleList();
        void DrawTypeDataPicker();
        void DrawDetails();
        void DrawCurve();
        bool DrawProperty(void* Object, const CoreUObject::FProperty& Property);
        bool DrawDistribution(void* Object, const CoreUObject::FProperty& Property);
        bool DrawArray(void* Object, const CoreUObject::FProperty& Property);
        bool DrawCurveTrack(Core::TArray<Engine::FFloatCurveKey>& Keys);

        void AddEmitter();
        void RemoveEmitter();
        void MoveEmitter(Index_t Target);
        void SetTypeData(const CoreUObject::UClass* Class);
        void AddModule(const CoreUObject::UClass& Class);
        void RemoveModule();
        void MoveModule(Index_t Target);
        void ChangeLOD(Index_t Target);

    private:
        Editor::FEditorContext& mContext;
        Asset::FAssetManager& mAssets;
        Editor::FEditorAssetCatalog& mCatalog;
        Editor::FAssetThumbnailCache& mThumbnails;
        Editor::FMessageBox& mMessages;
        Editor::FAssetPicker mPicker{};

        std::unique_ptr<Engine::UParticleSystem> mWorkingCopy{};
        Viewer::FAssetViewer mViewer;
        Editor::FParticleEditorPreview* mPreview{};
        Core::FString mError{};

        Editor::FParticleEditorToolkit::ESelection mSelection{Editor::FParticleEditorToolkit::ESelection::System};
        Index_t mEmitter{};
        Index_t mLOD{};
        Index_t mModule{};
        const CoreUObject::FProperty* mCurveProperty{};
        Int32 mCurveAxis{};
        Int32 mCurveKey{-1};
        FVector4 mCurveDragRange{};
        bool mCurveDragging{};

        float mSpeed{1.0f};
        float mFitDelay{};
        bool mPaused{};
        bool mOrbit{};
        bool mStepRequested{};
        bool mNeedsRestart{};
        bool mNeedsFit{true};
        bool mDirty{};
    };

}
