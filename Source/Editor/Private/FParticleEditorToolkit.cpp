#include "FParticleEditorToolkit.h"

#include "Asset/FAssetManager.h"
#include "Core/FilePath.h"
#include "Core/Literals/QuantityTypes.h"
#include "CoreUObject/Reflection/Cast.h"
#include "CoreUObject/Reflection/FReflectionRegistry.h"
#include "Editor/FEditorContext.h"
#include "Editor/FEditorToolkitHost.h"
#include "Engine/AParticleSystemActor.h"
#include "Engine/AStaticMeshActor.h"
#include "Engine/Component/UParticleSystemComponent.h"
#include "Engine/Component/UStaticMeshComponent.h"
#include "Engine/FProject.h"
#include "Engine/Particles/Module/UParticleModuleBeamNoise.h"
#include "Engine/Particles/Module/UParticleModuleMeshRotationRate.h"
#include "Engine/Particles/Module/UParticleModuleRequired.h"
#include "Engine/Particles/Module/UParticleModuleSpawn.h"
#include "Engine/Particles/Module/UParticleModuleTypeDataBeam.h"
#include "Engine/Particles/Module/UParticleModuleTypeDataMesh.h"
#include "Engine/Particles/Module/UParticleModuleTypeDataRibbon.h"
#include "Engine/Particles/ParticleAssets.h"
#include "Engine/Particles/UParticleEmitter.h"
#include "Engine/Particles/UParticleLODLevel.h"
#include "Engine/Particles/UParticleSystem.h"
#include "Engine/UWorld.h"
#include "Viewer/FPreviewScene.h"
#include "Viewer/IViewerContent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>
#include <limits>

using namespace Editor;

namespace {

    Core::FString ModuleLabel(const CoreUObject::UClass* Class) {
        if (Class == nullptr) {
            return "Sprite";
        }

        std::string_view Name{Class->GetName()};

        if (Name.starts_with("UParticleModuleTypeData")) {
            Name.remove_prefix(std::string_view{"UParticleModuleTypeData"}.size());
        } else if (Name.starts_with("UParticleModule")) {
            Name.remove_prefix(std::string_view{"UParticleModule"}.size());
        }

        Core::FString Label{};

        for (Index_t Index{}; Index < Name.size(); ++Index) {
            const char Character{Name[Index]};
            const bool Upper{Character >= 'A' && Character <= 'Z'};
            if (Index > 0 && Upper && Name[Index - 1] >= 'a' && Name[Index - 1] <= 'z') {
                Label += ' ';
            }
            Label += Character;
        }

        return Label;
    }

    bool CompatibleModule(const CoreUObject::UClass& Class, const Engine::UParticleModuleTypeDataBase* TypeData) {
        if (Class.IsChildOf(Engine::UParticleModuleBeamNoise::StaticClass())) {
            return TypeData != nullptr && TypeData->IsA<Engine::UParticleModuleTypeDataBeam>();
        }

        if (Class.IsChildOf(Engine::UParticleModuleMeshRotationRate::StaticClass())) {
            return TypeData != nullptr && TypeData->IsA<Engine::UParticleModuleTypeDataMesh>();
        }

        return true;
    }

    bool MatchingModules(const Engine::UParticleEmitter& Emitter) {
        const auto& First{Emitter.GetLODLevel(0)->GetModules()};

        return std::ranges::all_of(Emitter.GetLODLevels(), [&First](const auto& Level) {
            return std::ranges::equal(First, Level->GetModules(), [](const auto& Left, const auto& Right) {
                return Left->GetClass() == Right->GetClass() && Left->RequiredBytes() == Right->RequiredBytes();
            });
        });
    }

}

namespace Editor {

    class FParticleEditorPreview final : public Viewer::IViewerContent {
    public:
        FParticleEditorPreview(RenderCore::IRendererModule* Renderer, Asset::FAssetManager& Assets);

        ~FParticleEditorPreview() override;

    public:
        bool Initialize(PhysicsRuntime::FPhysicsRuntime* Physics, Core::FString& Error);
        bool SetTemplate(Engine::UParticleSystem* Template, Int32 LOD);
        void ResetParticles();
        void SetOrbit(bool Orbit);
        Int32 CountParticles() const;

        bool IsReady() const override;
        const FBox& GetBounds() const override;
        Engine::FSceneInterface* GetScene() const override;
        void Tick(float DeltaTime) override;
        void Synchronize() override;

    private:
        Viewer::FPreviewScene mScene;
        Engine::AParticleSystemActor* mActor{};
        FBox mBounds{{-0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 1.0f}};
        float mOrbitTime{};
        bool mOrbit{};
    };

}

FParticleEditorPreview::FParticleEditorPreview(RenderCore::IRendererModule* Renderer, Asset::FAssetManager& Assets)
	: mScene{Renderer, &Assets} {
}

FParticleEditorPreview::~FParticleEditorPreview() = default;

bool FParticleEditorPreview::Initialize(PhysicsRuntime::FPhysicsRuntime* Physics, Core::FString& Error) {
    if (!mScene.Initialize(Error)) {
        return false;
    }

    auto& World{*mScene.GetWorld()};
    mActor = CoreUObject::Cast<Engine::AParticleSystemActor>(World.SpawnActor(Engine::AParticleSystemActor::StaticClass()));

    if (mActor == nullptr) {
        Error = "Could not spawn the preview particle system.";
        return false;
    }

    mActor->SetActorLocation({0.0f, 0.0f, 0.35f});
    mActor->GetParticleSystemComponent()->SetEditorTickable(true);

    if (Physics != nullptr) {
        auto* Ground{World.SpawnPrimitive(Engine::EPrimitiveType::Cube)};

        if (Ground != nullptr) {
            Ground->SetActorTransform({{0.0f, 0.0f, -0.15f}, {}, {20.0f, 20.0f, 0.3f}});
            Ground->GetStaticMeshComponent()->SetCollisionPreset(PhysicsRuntime::ECollisionPreset::BlockAll);
            World.SetPhysicsRuntime(Physics);
            World.CreatePhysicsScene();
        }
    }

    return true;
}

bool FParticleEditorPreview::SetTemplate(Engine::UParticleSystem* Template, Int32 LOD) {
    auto& Component{*mActor->GetParticleSystemComponent()};

    if (!Component.SetPreviewTemplate(Template)) {
        return false;
    }

    Component.SetPreviewLODOverride(LOD);
    mOrbitTime = 0.0f;
    mActor->SetActorLocation({0.0f, 0.0f, 0.35f});

    return true;
}

void FParticleEditorPreview::ResetParticles() {
    mActor->GetParticleSystemComponent()->ResetParticles();
}

void FParticleEditorPreview::SetOrbit(bool Orbit) {
    mOrbit = Orbit;
}

Int32 FParticleEditorPreview::CountParticles() const {
    return mActor->GetParticleSystemComponent()->GetActiveParticleCount();
}

bool FParticleEditorPreview::IsReady() const {
    return mActor != nullptr && GetScene() != nullptr;
}

const FBox& FParticleEditorPreview::GetBounds() const {
    return mBounds;
}

Engine::FSceneInterface* FParticleEditorPreview::GetScene() const {
    return mScene.GetScene();
}

void FParticleEditorPreview::Tick(float DeltaTime) {
    if (DeltaTime <= 0.0f) {
        return;
    }

    if (mOrbit) {
        mOrbitTime += DeltaTime;
        mActor->SetActorLocation({std::cos(mOrbitTime * 2.0f) - 1.0f, std::sin(mOrbitTime * 2.0f), 0.65f});
    }

    mScene.GetWorld()->Tick(Core::FTime::FromBase(DeltaTime));
    mBounds = mActor->GetParticleSystemComponent()->GetWorldBounds();
}

void FParticleEditorPreview::Synchronize() {
    mScene.Synchronize();
}

FParticleEditorToolkit::FParticleEditorToolkit(FEditorContext& Context, const std::filesystem::path& Path, FEditorAssetCatalog& Catalog, FAssetThumbnailCache& Thumbnails, FMessageBox& Messages)
	: FAssetEditorToolkit{Path, Asset::EAssetType::ParticleSystem, "###ParticleEditor:" + Core::FilePath::ToGenericUtf8(Path)},
	  mContext{Context},
	  mAssets{*Context.GetProject()->GetAssets()},
	  mCatalog{Catalog},
	  mThumbnails{Thumbnails},
	  mMessages{Messages},
	  mViewer{Context.GetRenderer(), &mAssets} {
    auto& Host{GetToolkitHost()};
    Host.AddWidget("Preview", "Preview", [this](float) {
        DrawPreview();
    }, EEditorDockRegion::Center, false);
    Host.GetTabManager().FindTab("Preview")->SetWindowFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    Host.AddWidget("Emitters", "Emitters", [this](float) {
        DrawEmitters();
    }, EEditorDockRegion::Left, false);
    Host.AddWidget("Properties", "Details", [this](float) {
        DrawDetails();
    }, EEditorDockRegion::Details, false);
    Host.AddWidget("Curves", "Curves", [this](float) {
        DrawCurve();
    }, EEditorDockRegion::Bottom, false);
    GetTab().SetCanCloseHandler([this]() {
        return RequestClose();
    });
    mViewer.SetGridSpacing(0.25f);
    UpdateTitle();
}

FParticleEditorToolkit::~FParticleEditorToolkit() = default;

bool FParticleEditorToolkit::Open(Engine::UParticleSystem& Asset, Core::FString& Error) {
    mWorkingCopy = Asset.Duplicate(Error);

    if (mWorkingCopy == nullptr) {
        return false;
    }

    auto Preview{std::make_unique<FParticleEditorPreview>(mContext.GetRenderer(), mAssets)};
    auto* World{mContext.GetWorld()};

    if (!Preview->Initialize(World != nullptr ? World->GetPhysicsRuntime() : nullptr, Error)) {
        return false;
    }

    mPreview = Preview.get();

    if (!mViewer.SetContent(std::move(Preview))) {
        mPreview = nullptr;
        Error = mViewer.GetError();
        return false;
    }

    mOrbit = std::ranges::any_of(mWorkingCopy->GetEmitters(), [](const auto& Emitter) {
        return CoreUObject::Cast<Engine::UParticleModuleTypeDataRibbon>(Emitter->GetLODLevel(0)->GetTypeDataModule()) != nullptr;
    });

    mPreview->SetOrbit(mOrbit);
    RestartPreview();

    if (!mError.empty()) {
        Error = mError;
        return false;
    }

    return true;
}

void FParticleEditorToolkit::CloseRetired(const std::filesystem::path& Absolute) {
    Core::FString Error{};
    const auto OwnPath{Core::FilePath::ToLower(mAssets.ResolvePath(GetAssetPath(), Error))};
    const auto Relative{OwnPath.lexically_relative(Core::FilePath::ToLower(Absolute))};

    if (!Relative.empty() && *Relative.begin() != L"..") {
        GetTab().SetVisible(false);
    }
}

std::optional<FMessageBox::FSavePrompt> FParticleEditorToolkit::GetClosePrompt() {
    if (!mDirty || !GetTab().IsVisible()) {
        return {};
    }

    auto SaveAction{[this]() {
        return Save();
    }};
    auto DiscardAction{[this]() {
        mDirty = false;
        UpdateTitle();
        return true;
    }};

    return FMessageBox::FSavePrompt{"Particle System", "Save changes to " + Core::FilePath::ToUtf8(GetAssetPath().filename()) + "?", std::move(SaveAction), std::move(DiscardAction)};
}

bool FParticleEditorToolkit::RequestClose() {
    auto Prompt{GetClosePrompt()};

    if (!Prompt) {
        return true;
    }

    mMessages.ConfirmSave({std::move(*Prompt)}, [this]() {
        GetTab().SetVisible(false);
    });
    return false;
}

bool FParticleEditorToolkit::Save() {
    if (!mDirty) {
        return true;
    }

    mError.clear();

    if (!mAssets.SaveRegisteredAsset(GetAssetPath(), Asset::EAssetType::ParticleSystem, *mWorkingCopy, mError, true)) {
        mContext.AddLog(mError);
        return false;
    }

    mDirty = false;
    UpdateTitle();
    NotifyChanged();

    return true;
}

bool FParticleEditorToolkit::Reload(Core::FString& Error) {
    auto* Asset{Engine::LoadParticleSystem(mAssets, GetAssetPath(), Error)};
    auto Model{Asset != nullptr ? Asset->Duplicate(Error) : nullptr};

    if (Model == nullptr) {
        return false;
    }

    mPreview->ResetParticles();
    mWorkingCopy = std::move(Model);
    mLOD = 0;
    mEmitter = 0;
    Select(ESelection::System);
    RestartPreview();
    mDirty = false;
    UpdateTitle();

    return true;
}

void FParticleEditorToolkit::UpdateTitle() {
    const Core::FString Id{Core::FilePath::ToGenericUtf8(GetAssetPath())};
    GetTab().SetName(Core::FilePath::ToUtf8(GetAssetPath().stem()) + (mDirty ? " *" : "") + "###ParticleEditor:" + Id);
}

void FParticleEditorToolkit::HandleCommands() {
    if (mDirty && mContext.CanEditWorld() && GetTab().IsFocused() && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
        Save();
    }
}

void FParticleEditorToolkit::UpdateEditor(float DeltaTime) {
    if (mNeedsRestart) {
        RestartPreview();
    }

    if (mStepRequested || !mPaused) {
        const float SimulationDelta{mStepRequested ? 1.0f / 60.0f : std::clamp(DeltaTime, 0.0f, 0.1f) * mSpeed};

        for (float Remaining{SimulationDelta}; Remaining > 0.0f;) {
            const float Step{std::min(Remaining, 1.0f / 30.0f)};

            mViewer.Tick(Step);
            Remaining -= Step;
        }

        if (mNeedsFit) {
            mFitDelay += SimulationDelta;
        }
    }

    mStepRequested = false;
}

void FParticleEditorToolkit::RestartPreview() {
    mPreview->ResetParticles();
    mWorkingCopy->BuildEmitters();
    mLOD = std::min(mLOD, mWorkingCopy->GetNumLODs() - 1);

    if (!mPreview->SetTemplate(mWorkingCopy.get(), static_cast<Int32>(mLOD))) {
        mError = "Could not start the particle preview.";
    }

    mNeedsRestart = false;
}

void FParticleEditorToolkit::MarkDirty() {
    mNeedsRestart = true;
    mError.clear();

    if (!mDirty) {
        mDirty = true;
        UpdateTitle();
    }
}

void FParticleEditorToolkit::ResetCurveSelection() {
    mCurveProperty = nullptr;
    mCurveAxis = 0;
    mCurveKey = -1;
    mCurveDragging = false;
}

void FParticleEditorToolkit::Select(ESelection Selection, Index_t Module) {
    mSelection = Selection;
    mModule = Module;
    ResetCurveSelection();
}

Engine::UParticleEmitter* FParticleEditorToolkit::GetEmitter() const {
    return mWorkingCopy->GetEmitter(mEmitter);
}

Engine::UParticleLODLevel* FParticleEditorToolkit::GetLevel() const {
    auto* Emitter{GetEmitter()};
    return Emitter != nullptr && Emitter->GetNumLODLevels() > 0 ? Emitter->GetLODLevel(std::min(mLOD, Emitter->GetNumLODLevels() - 1)) : nullptr;
}

CoreUObject::UObject* FParticleEditorToolkit::GetSelectedObject() const {
    if (mSelection == ESelection::System) {
        return mWorkingCopy.get();
    }

    if (mSelection == ESelection::Emitter) {
        return GetEmitter();
    }

    auto* Level{GetLevel()};

    if (Level == nullptr) {
        return nullptr;
    }

    switch (mSelection) {
    case ESelection::Required:
        return Level->GetRequiredModule();
    case ESelection::Spawn:
        return Level->GetSpawnModule();
    case ESelection::TypeData:
        return Level->GetTypeDataModule();
    case ESelection::Module:
        return mModule < Level->GetModules().size() ? Level->GetModules()[mModule].get() : nullptr;
    default:
        return nullptr;
    }
}

void FParticleEditorToolkit::DrawPreview() {
    ImGui::BeginDisabled(!mContext.CanEditWorld() || !mDirty);
    if (ImGui::Button("Save")) {
        Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert")) {
        ImGui::OpenPopup("Revert Particle System");
    }
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal("Revert Particle System", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Discard unsaved edits and reload the saved particle system?");
        if (ImGui::Button("Revert Edits")) {
            mError.clear();
            Reload(mError);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(mPaused ? "Play" : "Pause")) {
        mPaused = !mPaused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart")) {
        mNeedsRestart = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!mPaused);
    if (ImGui::Button("Step")) {
        mStepRequested = true;
    }
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("Speed", &mSpeed, 0.05f, 4.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Checkbox("Move Emitter", &mOrbit)) {
        mPreview->SetOrbit(mOrbit);
        mNeedsRestart = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        mNeedsFit = true;
        mFitDelay = 0.25f;
    }

    ImGui::Text("LOD %zu  |  %d particles", mLOD, mPreview->CountParticles());

    if (!mError.empty()) {
        ImGui::TextWrapped("%s", mError.c_str());
    }

    const ImVec2 Size{ImGui::GetContentRegionAvail()};
    if (Size.x < 1.0f || Size.y < 1.0f || !mViewer.Resize(static_cast<Uint32>(Size.x), static_cast<Uint32>(Size.y))) {
        return;
    }

    if (mNeedsFit && (mFitDelay >= 0.25f || mPaused)) {
        mNeedsFit = !mViewer.Fit(1.3f);
    }

    const ImVec2 Origin{ImGui::GetCursorScreenPos()};
    ImGui::InvisibleButton("Particle View", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    auto& Camera{mViewer.GetCamera()};
    const auto& IO{ImGui::GetIO()};

    if (ImGui::IsItemHovered() && IO.MouseWheel != 0.0f) {
        Camera.Zoom(IO.MouseWheel);
    }

    if (ImGui::IsItemActive()) {
        const FVector2 Delta{IO.MouseDelta.x, IO.MouseDelta.y};
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            Camera.Pan(Delta, Size.y);
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            Camera.Orbit(Delta);
        }
    }

    if (mViewer.Render()) {
        const ImTextureRef Texture{static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mViewer.GetNativeTexture()))};
        ImGui::GetWindowDrawList()->AddImage(Texture, Origin, {Origin.x + Size.x, Origin.y + Size.y});
    }
}

void FParticleEditorToolkit::DrawLODControls() {
    ImGui::TextUnformatted("LOD");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(88.0f);
    const Core::FString Current{"LOD " + std::to_string(mLOD)};

    if (ImGui::BeginCombo("##LOD", Current.c_str())) {
        for (Index_t Index{}; Index < mWorkingCopy->GetNumLODs(); ++Index) {
            const Core::FString Label{"LOD " + std::to_string(Index)};

            if (ImGui::Selectable(Label.c_str(), Index == mLOD)) {
                ChangeLOD(Index);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("+##LOD")) {
        mPreview->ResetParticles();
        if (mWorkingCopy->InsertLOD(mLOD + 1)) {
            ChangeLOD(mLOD + 1);
            MarkDirty();
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(mLOD == 0);
    if (ImGui::SmallButton("-##LOD")) {
        mPreview->ResetParticles();
        if (mWorkingCopy->RemoveLOD(mLOD)) {
            ChangeLOD(mLOD - 1);
            MarkDirty();
        }
    }
    ImGui::EndDisabled();

    float Distance{mWorkingCopy->GetLODDistance(mLOD)};
    ImGui::SetNextItemWidth(95.0f);
    ImGui::BeginDisabled(mLOD == 0);
    if (ImGui::DragFloat("Distance (m)", &Distance, 0.1f) && std::isfinite(Distance)) {
        const float Minimum{std::nextafter(mWorkingCopy->GetLODDistance(mLOD - 1), std::numeric_limits<float>::infinity())};
        const float Maximum{mLOD + 1 < mWorkingCopy->GetNumLODs() ? std::nextafter(mWorkingCopy->GetLODDistance(mLOD + 1), 0.0f) : std::numeric_limits<float>::max()};

        if (Minimum <= Maximum) {
            mWorkingCopy->SetLODDistance(mLOD, std::clamp(Distance, Minimum, Maximum));
            MarkDirty();
        }
    }
    ImGui::EndDisabled();
}

void FParticleEditorToolkit::DrawEmitters() {
    ImGui::BeginDisabled(!mContext.CanEditWorld());
    DrawLODControls();
    ImGui::Separator();
    if (ImGui::Selectable("System Settings", mSelection == ESelection::System)) {
        Select(ESelection::System);
    }

    for (Index_t Index{}; Index < mWorkingCopy->GetNumEmitters(); ++Index) {
        ImGui::PushID(static_cast<int>(Index));
        auto* Emitter{mWorkingCopy->GetEmitter(Index)};
        const auto* TypeData{Emitter->GetLODLevel(0)->GetTypeDataModule()};
        const Core::FString Name{Emitter->GetName().empty() ? "Emitter " + std::to_string(Index + 1) : Emitter->GetName()};
        const Core::FString Label{Name + "  (" + ModuleLabel(TypeData != nullptr ? TypeData->GetClass() : nullptr) + ")###Emitter"};

        if (ImGui::Selectable(Label.c_str(), mSelection != ESelection::System && mEmitter == Index)) {
            mEmitter = Index;
            Select(ESelection::Emitter);
        }
        ImGui::PopID();
    }

    if (ImGui::SmallButton("Add Emitter")) {
        AddEmitter();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(GetEmitter() == nullptr);
    if (ImGui::SmallButton("Remove##Emitter")) {
        RemoveEmitter();
    }
    ImGui::EndDisabled();

    if (auto* Level{GetLevel()}) {
        ImGui::BeginDisabled(mEmitter == 0);
        if (ImGui::SmallButton("Up##Emitter")) {
            MoveEmitter(mEmitter - 1);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(mEmitter + 1 >= mWorkingCopy->GetNumEmitters());
        if (ImGui::SmallButton("Down##Emitter")) {
            MoveEmitter(mEmitter + 1);
        }
        ImGui::EndDisabled();
        ImGui::SeparatorText("Modules");

        bool Enabled{Level->IsEnabled()};

        if (ImGui::Checkbox("Enable this emitter at current LOD", &Enabled)) {
            Level->SetEnabled(Enabled);
            MarkDirty();
        }

        DrawTypeDataPicker();
        DrawModuleList();
    }
    ImGui::EndDisabled();
}

void FParticleEditorToolkit::DrawTypeDataPicker() {
    const auto* TypeData{GetLevel()->GetTypeDataModule()};
    const auto* Current{TypeData != nullptr ? TypeData->GetClass() : nullptr};
    const auto Label{ModuleLabel(Current)};
    const std::array<const CoreUObject::UClass*, 4> Types{nullptr, Engine::UParticleModuleTypeDataMesh::StaticClass(), Engine::UParticleModuleTypeDataBeam::StaticClass(), Engine::UParticleModuleTypeDataRibbon::StaticClass()};

    if (ImGui::BeginCombo("Emitter Type", Label.c_str())) {
        for (const auto* Type : Types) {
            if (ImGui::Selectable(ModuleLabel(Type).c_str(), Type == Current) && Type != Current) {
                SetTypeData(Type);
            }
        }
        ImGui::EndCombo();
    }
}

void FParticleEditorToolkit::DrawModuleList() {
    auto* Level{GetLevel()};
    if (ImGui::Selectable("Required", mSelection == ESelection::Required)) {
        Select(ESelection::Required);
    }
    if (ImGui::Selectable("Spawn", mSelection == ESelection::Spawn)) {
        Select(ESelection::Spawn);
    }
    if (Level->GetTypeDataModule() != nullptr && ImGui::Selectable("Type Data", mSelection == ESelection::TypeData)) {
        Select(ESelection::TypeData);
    }

    const auto& Modules{Level->GetModules()};
    for (Index_t Index{}; Index < Modules.size(); ++Index) {
        ImGui::PushID(static_cast<int>(Index));
        auto& Module{*Modules[Index]};
        bool Enabled{Module.IsEnabled()};
        if (ImGui::Checkbox("##Enabled", &Enabled)) {
            Module.SetEnabled(Enabled);
            MarkDirty();
        }
        ImGui::SameLine();
        const bool Compatible{CompatibleModule(*Module.GetClass(), Level->GetTypeDataModule())};
        const Core::FString Label{ModuleLabel(Module.GetClass()) + (Compatible ? "" : " (inactive)")};
        if (ImGui::Selectable(Label.c_str(), mSelection == ESelection::Module && mModule == Index)) {
            Select(ESelection::Module, Index);
        }
        ImGui::PopID();
    }

    const bool Matching{MatchingModules(*GetEmitter())};
    ImGui::BeginDisabled(!Matching);
    if (ImGui::SmallButton("Add Module")) {
        ImGui::OpenPopup("Particle Modules");
    }

    if (ImGui::BeginPopup("Particle Modules")) {
        for (const auto* Class : CoreUObject::FReflectionRegistry::GetDerivedClasses(Engine::UParticleModule::StaticClass())) {
            if (!Class->CanCreate() || Class == Engine::UParticleModuleRequired::StaticClass() || Class == Engine::UParticleModuleSpawn::StaticClass() || Class->IsChildOf(Engine::UParticleModuleTypeDataBase::StaticClass()) || !CompatibleModule(*Class, Level->GetTypeDataModule())) {
                continue;
            }

            if (ImGui::MenuItem(ModuleLabel(Class).c_str())) {
                AddModule(*Class);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(mSelection != ESelection::Module);
    if (ImGui::SmallButton("Remove##Module")) {
        RemoveModule();
    }
    ImGui::BeginDisabled(mModule == 0);
    if (ImGui::SmallButton("Up##Module")) {
        MoveModule(mModule - 1);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mModule + 1 >= Modules.size());
    if (ImGui::SmallButton("Down##Module")) {
        MoveModule(mModule + 1);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (!Matching) {
        ImGui::TextWrapped("Module layout differs between LODs. Module order is shared by the runtime.");
    }
}

void FParticleEditorToolkit::AddEmitter() {
    auto Emitter{Engine::UParticleEmitter::CreateDefault()};
    Emitter->SetName("Emitter " + std::to_string(mWorkingCopy->GetNumEmitters() + 1));

    for (Index_t Index{1}; Index < mWorkingCopy->GetNumLODs(); ++Index) {
        if (Emitter->DuplicateLODLevel(0, Index) == nullptr) {
            mError = "Could not copy the emitter to every LOD.";
            return;
        }
    }

    mEmitter = mWorkingCopy->GetNumEmitters();
    mWorkingCopy->AddEmitter(std::move(Emitter));
    Select(ESelection::Emitter);
    MarkDirty();
}

void FParticleEditorToolkit::RemoveEmitter() {
    mPreview->ResetParticles();

    if (mWorkingCopy->RemoveEmitter(GetEmitter()) != nullptr) {
        mEmitter = std::min(mEmitter, mWorkingCopy->GetNumEmitters() > 0 ? mWorkingCopy->GetNumEmitters() - 1 : Index_t{});
        Select(GetEmitter() != nullptr ? ESelection::Emitter : ESelection::System);
        MarkDirty();
    }
}

void FParticleEditorToolkit::MoveEmitter(Index_t Target) {
    if (mWorkingCopy->MoveEmitter(mEmitter, Target)) {
        mEmitter = Target;
        MarkDirty();
    }
}

void FParticleEditorToolkit::SetTypeData(const CoreUObject::UClass* Class) {
    const auto& Levels{GetEmitter()->GetLODLevels()};
    Core::TArray<std::unique_ptr<Engine::UParticleModuleTypeDataBase>> Modules{};
    Modules.reserve(Levels.size());

    for (const auto& Level : Levels) {
        const auto* Current{Level->GetTypeDataModule()};
        auto Object{Class != nullptr && (Current == nullptr || Current->GetClass() != Class) ? Class->CreateInstance() : nullptr};

        if (Class != nullptr && (Current == nullptr || Current->GetClass() != Class) && Object == nullptr) {
            mError = "Could not create the selected emitter type.";
            return;
        }

        Modules.emplace_back(static_cast<Engine::UParticleModuleTypeDataBase*>(Object.release()));
    }

    mPreview->ResetParticles();

    for (Index_t Index{}; Index < Levels.size(); ++Index) {
        if (Class == nullptr || Modules[Index] != nullptr) {
            Levels[Index]->SetTypeDataModule(std::move(Modules[Index]));
        }
    }

    Select(Class != nullptr ? ESelection::TypeData : ESelection::Emitter);
    MarkDirty();
}

void FParticleEditorToolkit::AddModule(const CoreUObject::UClass& Class) {
    const auto& Levels{GetEmitter()->GetLODLevels()};
    Core::TArray<std::unique_ptr<Engine::UParticleModule>> Modules{};
    Modules.reserve(Levels.size());

    for (Index_t Index{}; Index < Levels.size(); ++Index) {
        auto Object{Class.CreateInstance()};
        if (Object == nullptr) {
            mError = "Could not create the selected module.";
            return;
        }
        Modules.emplace_back(static_cast<Engine::UParticleModule*>(Object.release()));
    }

    mPreview->ResetParticles();

    const Index_t Slot{GetLevel()->GetModules().size()};
    for (Index_t Index{}; Index < Levels.size(); ++Index) {
        Levels[Index]->AddModule(std::move(Modules[Index]));
    }

    Select(ESelection::Module, Slot);
    MarkDirty();
}

void FParticleEditorToolkit::RemoveModule() {
    mPreview->ResetParticles();

    for (const auto& Level : GetEmitter()->GetLODLevels()) {
        Level->RemoveModule(Level->GetModules()[mModule].get());
    }

    const Index_t Count{GetLevel()->GetModules().size()};
    Select(Count > 0 ? ESelection::Module : ESelection::Emitter, Count > 0 ? std::min(mModule, Count - 1) : Index_t{});
    MarkDirty();
}

void FParticleEditorToolkit::MoveModule(Index_t Target) {
    mPreview->ResetParticles();

    for (const auto& Level : GetEmitter()->GetLODLevels()) {
        Level->MoveModule(mModule, Target);
    }

    mModule = Target;
    MarkDirty();
}

void FParticleEditorToolkit::ChangeLOD(Index_t Target) {
    mLOD = Target;
    mNeedsRestart = true;
    ResetCurveSelection();
}
