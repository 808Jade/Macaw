/**
 * @file FAssetEditorManager.cpp
 * @brief 에셋 종류별 편집기를 열고 활성 탭과 편집기 수명을 관리합니다.
 * @ingroup Editor
 */

#include "FAssetEditorManager.h"
#include "FAssetEditorToolkit.h"
#include "FMaterialEditorToolkit.h"
#include "FTextureEditorToolkit.h"
#include "FEditorWorkspace.h"
#include "FEditorCommands.h"
#include "Core/FilePath.h"
#include "Editor/FEditorContext.h"
#include "Editor/FAssetThumbnailCache.h"
#include "Editor/FMeshViewer.h"
#include "FBlendSpaceEditor.h"
#include "FAnimGraphEditor.h"
#include "FAnimSequenceEditor.h"
#include "Engine/FProject.h"
#include "Engine/AActor.h"
#include "Engine/Component/USkeletalMeshComponent.h"
#include "CoreUObject/Reflection/Cast.h"
#include "Asset/UMaterial.h"
#include "Asset/UTexture2D.h"
#include "Platform/Window.h"
#include "RenderCore/IRendererModule.h"
#include "EditorStyle.h"
#include <imgui_internal.h>
#include <d3d11.h>
#include <algorithm>
#include <variant>
#include <type_traits>

using namespace Editor;

/**
 * @brief FAssetEditorManager 구현에 한정되는 내부 형식·상수·도우미 함수입니다.
 */
namespace {
    /**
     * @brief 메시·애니메이션 편집기를 공통 에셋 툴킷 인터페이스에 연결합니다.
     */
    class FHostedAssetEditorToolkit final : public FAssetEditorToolkit {
    public:
        /**
         * @brief 호스트 툴킷이 소유할 수 있는 메시·애니메이션 편집기 형식 집합입니다.
         */
        using FContent = std::variant<std::unique_ptr<FMeshViewer>, std::unique_ptr<FAnimGraphEditor>, std::unique_ptr<FBlendSpaceEditor>, std::unique_ptr<FAnimSequenceEditor>>;

    public:
        /**
         * @brief FHostedAssetEditorToolkit의 초기 상태와 전달받은 의존성을 구성합니다.
         * @param[in] Context 편집 대상과 공유 서비스를 제공하는 에디터 컨텍스트의 비소유 주소입니다.
         * @param[in] Platform 창·입력·파일 대화상자를 제공하는 플랫폼 애플리케이션입니다.
         * @param[in] Path 대상 에셋 또는 콘텐츠 경로입니다.
         * @param[in] Type 편집할 에셋의 종류입니다.
         * @param[in] WindowName 탭 콘텐츠에 사용할 고유 ImGui 창 이름입니다.
         * @param[in] Content 호스트 툴킷이 소유할 메시 또는 애니메이션 편집기입니다.
         */
        FHostedAssetEditorToolkit(FEditorContext* Context, Platform::FWindowsApplication* Platform, const std::filesystem::path& Path, Asset::EAssetType Type, Core::FString WindowName, FContent Content);

        /**
         * @brief 객체를 소멸하고 소유 멤버와 연결 상태를 정리합니다.
         */
        ~FHostedAssetEditorToolkit() override;

    public:
        /**
         * @brief 폐기된 에셋 경로를 참조하는 편집기 콘텐츠를 닫거나 해제합니다.
         * @param[in] Absolute 폐기 또는 경로 작업 대상의 절대 경로입니다.
         */
        void CloseRetired(const std::filesystem::path& Absolute) override;

        /**
         * @brief 호스팅한 에셋 탭을 그리고 그래프 편집기의 후속 저장을 처리합니다.
         * @param[in] DeltaTime 이전 프레임 이후 경과한 시간이며 단위는 초입니다.
         */
        void Draw(float DeltaTime) override;

        /**
         * @brief 애니메이션 시퀀스 편집기의 미저장 변경을 확인하고 다른 편집기는 닫기를 허용합니다.
         * @return 콘텐츠가 닫기를 허용하면 true입니다.
         */
        bool CanClose() override;

    private:
        /**
         * @brief 에셋 편집기의 프레임별 상태를 갱신합니다.
         * @param[in] DeltaTime 이전 프레임 이후 경과한 시간이며 단위는 초입니다.
         */
        void UpdateEditor(float DeltaTime) override;

        /**
         * @brief 메시 뷰어를 제외한 호스팅 편집기의 저장 단축키를 처리합니다.
         */
        void HandleCommands() override;

    private:
        /**
         * @brief 편집 대상과 공유 서비스에 접근하는 에디터 컨텍스트의 비소유 주소입니다.
         */
        FEditorContext* mContext{};

        /**
         * @brief Win32 창·입력·파일 선택을 제공하는 플랫폼 애플리케이션입니다.
         */
        Platform::FWindowsApplication* mPlatform{};

        /**
         * @brief 호스팅한 메시 또는 애니메이션 편집기의 소유 객체입니다.
         */
        FContent mContent{};
    };

    FHostedAssetEditorToolkit::FHostedAssetEditorToolkit(FEditorContext* Context, Platform::FWindowsApplication* Platform, const std::filesystem::path& Path, Asset::EAssetType Type, Core::FString WindowName, FContent Content)
		: FAssetEditorToolkit{Path, Type, std::move(WindowName)},
		  mContext{Context},
		  mPlatform{Platform},
		  mContent{std::move(Content)} {
        std::visit([this](auto& Content) {
            using FContentType = std::remove_reference_t<decltype(*Content)>;

            if constexpr (std::is_same_v<FContentType, FMeshViewer>) {
                Content->InitializeToolkit(GetTab(), mPlatform->GetInput());
            } else {
                Content->InitializeToolkit(GetTab());
            }
        },
            mContent);
        GetTab().SetCanCloseHandler([this]() {
            return CanClose();
        });
    }

    FHostedAssetEditorToolkit::~FHostedAssetEditorToolkit() = default;

    void FHostedAssetEditorToolkit::UpdateEditor(float DeltaTime) {
        std::visit([this, DeltaTime](auto& Content) {
            Content->UpdateToolkit(DeltaTime, mContext->CanEditWorld());

            if (!Content->IsOpen()) {
                GetTab().SetVisible(false);
            }
        },
            mContent);
    }

    void FHostedAssetEditorToolkit::HandleCommands() {
        std::visit([this](auto& Content) {
            using FContentType = std::remove_reference_t<decltype(*Content)>;

            if constexpr (!std::is_same_v<FContentType, FMeshViewer>) {
                Content->HandleCommands(mContext->CanEditWorld());
            }
        },
            mContent);
    }

    void FHostedAssetEditorToolkit::Draw(float DeltaTime) {
        FAssetEditorToolkit::Draw(DeltaTime);
        std::visit([](auto& Content) {
            using FContentType = std::remove_reference_t<decltype(*Content)>;

            if constexpr (std::is_same_v<FContentType, FAnimGraphEditor>) {
                Content->FinishToolkit();
            }
        },
            mContent);
    }

    bool FHostedAssetEditorToolkit::CanClose() {
        return std::visit([](auto& Content) {
            using FContentType = std::remove_reference_t<decltype(*Content)>;

            if constexpr (std::is_same_v<FContentType, FAnimSequenceEditor>) {
                return Content->RequestClose();
            } else {
                return true;
            }
        },
            mContent);
    }

    void FHostedAssetEditorToolkit::CloseRetired(const std::filesystem::path& Absolute) {
        std::visit([&Absolute](auto& Content) {
            Content->CloseRetired(Absolute);
        },
            mContent);
    }
}

FAssetEditorManager::FAssetEditorManager(FEditorContext* Context, Platform::FWindowsApplication* Platform, FEditorAssetCatalog& Catalog, FAssetThumbnailCache& Thumbnails, FEditorWorkspace& Workspace, FEditorCommands& Commands)
	: mContext{Context},
	  mPlatform{Platform},
	  mCatalog{Catalog},
	  mThumbnails{Thumbnails},
	  mWorkspace{Workspace},
	  mCommands{Commands},
	  mScriptContent{"ScriptIDE", [this](float) {
          mScriptIDE.Draw();
          if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
              mScriptIDE.Save();
          }
      }} {
    mScriptTab.SetContent(&mScriptContent);
    mScriptTab.SetContentPadding({});
    mScriptTab.SetVisible(false);
    mScriptTab.SetCanCloseHandler([this]() {
        return ConfirmScriptChange();
    });
    mScriptTab.SetClosedHandler([this]() {
        mScriptIDE.DiscardChanges();
    });
}

FAssetEditorManager::~FAssetEditorManager() = default;

FAssetEditorToolkit* FAssetEditorManager::FindActiveEditor() const {
    FAssetEditorToolkit* Active{};

    for (const auto& Editor : mEditors) {
        FEditorTab& Tab{Editor->GetTab()};
        const ImGuiWindow* Window{ImGui::FindWindowByName(Tab.GetName().c_str())};

        if (!Tab.IsVisible() || Window == nullptr || Window->Hidden || Window->Collapsed) {
            continue;
        }

        if (Tab.IsFocused()) {
            return Editor.get();
        }

        if (Editor.get() == mActiveEditor) {
            Active = Editor.get();
        }
    }

    return Active;
}

FEditorToolkitHost* FAssetEditorManager::GetActiveToolkitHost() const {
    FAssetEditorToolkit* Editor{FindActiveEditor()};

    return Editor != nullptr ? &Editor->GetToolkitHost() : nullptr;
}

void FAssetEditorManager::BrowseActiveAsset() {
    if (const FAssetEditorToolkit* Editor{FindActiveEditor()}) {
        EditorStyle::BrowseAsset(Editor->GetAssetPath());
    }
}

void FAssetEditorManager::SetChangedHandler(std::function<void()> Handler) {
    mChanged = std::move(Handler);
}

bool FAssetEditorManager::HasOpenMaterial(const std::function<bool(const std::filesystem::path&)>& Predicate) const {
    return std::any_of(mEditors.begin(), mEditors.end(), [&Predicate](const auto& Editor) {
        return Editor->GetAssetType() == Asset::EAssetType::Material && Predicate(Editor->GetAssetPath());
    });
}

bool FAssetEditorManager::HasUnsavedScript(const std::function<bool(const std::filesystem::path&)>& Predicate) const {
    return mScriptTab.IsVisible() && mScriptIDE.IsDirty() && Predicate(mScriptIDE.GetPath());
}

bool FAssetEditorManager::IsFocused() const {
    return (mScriptTab.IsVisible() && mScriptTab.IsFocused()) || std::any_of(mEditors.begin(), mEditors.end(), [](const auto& Editor) {
        return Editor->GetTab().IsFocused();
    });
}

bool FAssetEditorManager::CanClose() {
    return ConfirmScriptChange() && std::all_of(mEditors.begin(), mEditors.end(), [](const auto& Editor) {
        return Editor->CanClose();
    });
}

bool FAssetEditorManager::ConfirmScriptChange() {
    if (!mScriptTab.IsVisible() || !mScriptIDE.IsDirty()) {
        return true;
    }

    const std::wstring Message{L"Save changes to " + mScriptIDE.GetPath().filename().wstring() + L"?"};
    const Int32 Choice{MessageBoxW(mPlatform->GetWindowHandle(), Message.c_str(), L"Script IDE", MB_YESNOCANCEL | MB_ICONQUESTION)};

    if (Choice != IDYES) {
        return Choice == IDNO;
    }

    if (!mScriptIDE.Save()) {
        mScriptTab.Open();
        return false;
    }

    return true;
}

void FAssetEditorManager::CloseRetired(const std::filesystem::path& Absolute) {
    if (mScriptTab.IsVisible()) {
        const auto Relative{Core::FilePath::ToLower(mScriptIDE.GetPath()).lexically_relative(Core::FilePath::ToLower(Absolute))};
        if (!Relative.empty() && *Relative.begin() != L"..") {
            mScriptTab.SetVisible(false);
        }
    }

    for (auto& Editor : mEditors) {
        Editor->CloseRetired(Absolute);
    }
}

Core::FString FAssetEditorManager::GetTabId(const FAssetEditorToolkit& Toolkit) const {
    return Core::FilePath::ToGenericUtf8(Toolkit.GetAssetPath()) + "|" + std::to_string(static_cast<Uint32>(Toolkit.GetAssetType()));
}

void FAssetEditorManager::Draw(float DeltaTime) {
    mTabs.Draw(DeltaTime);

    if (mScriptTab.IsVisible()) {
        constexpr ImGuiWindowFlags Flags{ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse};
        mScriptTab.SetWindowFlags(Flags | (mScriptIDE.IsDirty() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None));
        mWorkspace.BeginMajorTab();
        mScriptTab.Draw(DeltaTime);
        mWorkspace.EndMajorTab();
    }

    if (mWorkspace.IsLevelEditorFocused() || (mScriptTab.IsVisible() && mScriptTab.IsFocused())) {
        mActiveEditor = nullptr;
    }

    for (const auto& Editor : mEditors) {
        if (Editor->GetTab().IsFocused()) {
            mActiveEditor = Editor.get();
        }
    }

    std::erase_if(mEditors, [this](const auto& Editor) {
        if (Editor->GetTab().IsVisible()) {
            return false;
        }

        if (mActiveEditor == Editor.get()) {
            mActiveEditor = nullptr;
        }

        mTabs.UnregisterTab(GetTabId(*Editor));
        return true;
    });
}

void FAssetEditorManager::OpenLuaScript(const std::filesystem::path& Path) {
    Core::FString Error{};
    const auto Absolute{mContext->GetProject()->GetAssets()->ResolvePath(Path, Error)};

    if (Absolute.empty()) {
        mContext->AddLog("Could not open Lua script " + Core::FilePath::ToGenericUtf8(Path) + (Error.empty() ? "" : ": " + Error));
        return;
    }

    if (mScriptIDE.IsDirty() && Core::FilePath::ToLower(Absolute) == Core::FilePath::ToLower(mScriptIDE.GetPath())) {
        mScriptTab.Open();
        return;
    }

    if (!ConfirmScriptChange()) {
        return;
    }

    if (!mScriptIDE.Open(Absolute)) {
        mContext->AddLog("Could not read Lua script " + Core::FilePath::ToGenericUtf8(Path));
        return;
    }

    mScriptTab.SetName("Script IDE - " + Core::FilePath::ToUtf8(Path.filename()) + "###ScriptIDE");
    mScriptTab.Open();
}

void FAssetEditorManager::OpenContentAsset(const std::filesystem::path& Path, Asset::EAssetType Type) {
    if (Type == Asset::EAssetType::Scene) {
        if (mCommands.ConfirmSceneChange()) {
            mContext->LoadScene(Path);
        }
        return;
    }

    for (auto& Editor : mEditors) {
        if (Editor->GetAssetPath() == Path && Editor->GetAssetType() == Type) {
            Editor->GetTab().Open();
            return;
        }
    }

    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};
    Core::FString Error{};
    const Core::FString Key{Core::FilePath::ToGenericUtf8(Path)};
    std::unique_ptr<FAssetEditorToolkit> Toolkit{};
    if (Type == Asset::EAssetType::Material) {
        if (auto* Material{Assets->LoadMaterial(Path, Error)}) {
            Toolkit = std::make_unique<FMaterialEditorToolkit>(mContext, mPlatform, Path, Material, mCatalog, mThumbnails);
        }
    } else if (Type == Asset::EAssetType::Texture) {
        if (Asset::UTexture2D* Texture{Assets->LoadTexture(Path, Error)}; Texture != nullptr && Texture->CreateResources(static_cast<ID3D11Device*>(mContext->GetRenderer()->GetNativeDevice()))) {
            Toolkit = std::make_unique<FTextureEditorToolkit>(mContext, Path, Texture);
        }
    } else if (Type == Asset::EAssetType::Mesh || Type == Asset::EAssetType::SkeletalMesh) {
        auto Content{std::make_unique<FMeshViewer>(mContext->GetRenderer(), Assets)};
        Content->SetWindowId("###Mesh Viewer:" + Key);
        if (Content->Open(Path, Error)) {
            const Core::FString Name{Content->GetWindowId()};
            Toolkit = std::make_unique<FHostedAssetEditorToolkit>(mContext, mPlatform, Path, Type, Name, std::move(Content));
        }
    } else if (Type == Asset::EAssetType::AnimGraph) {
        auto Content{std::make_unique<FAnimGraphEditor>(Assets, &mCatalog, &mThumbnails)};
        Content->SetWindowId("###AnimGraphEditor:" + Key);
        if (Content->Open(Path, Error)) {
            const Core::FString Name{Content->GetWindowId()};
            Toolkit = std::make_unique<FHostedAssetEditorToolkit>(mContext, mPlatform, Path, Type, Name, std::move(Content));
        }
    } else if (Type == Asset::EAssetType::BlendSpace) {
        auto Content{std::make_unique<FBlendSpaceEditor>(Assets, &mCatalog, &mThumbnails)};
        Content->SetWindowId("###BlendSpaceEditor:" + Key);
        if (Content->Open(Path, Error)) {
            const Core::FString Name{Content->GetWindowId()};
            Toolkit = std::make_unique<FHostedAssetEditorToolkit>(mContext, mPlatform, Path, Type, Name, std::move(Content));
        }
    } else if (Type == Asset::EAssetType::AnimSequence) {
        auto Content{std::make_unique<FAnimSequenceEditor>(mContext->GetRenderer(), Assets, &mCatalog, &mThumbnails)};
        Content->SetWindowId("###AnimSequenceEditor:" + Key);
        if (Content->Open(Path, Error)) {
            Core::FString PreviewMesh{};
            Core::FString PreviewSkeleton{};
            Engine::AActor* Actor{mContext->GetSelectedActor()};
            auto* Component{Actor != nullptr ? CoreUObject::Cast<Engine::USkeletalMeshComponent>(Actor->GetRootComponent()) : nullptr};
            if (Component != nullptr && !mThumbnails.GetAnimationPreview(Path, PreviewMesh, PreviewSkeleton)) {
                Content->SetPreviewMesh(Component->GetMeshPath(), Component->GetSkeletonPath());
            }
            const Core::FString Name{Content->GetWindowId()};
            Toolkit = std::make_unique<FHostedAssetEditorToolkit>(mContext, mPlatform, Path, Type, Name, std::move(Content));
        }
    }

    if (Toolkit != nullptr) {
        Toolkit->SetChangedHandler([this]() {
            if (mChanged) {
                mChanged();
            }
        });
        Toolkit->GetTab().Open();
        FAssetEditorToolkit* Opened{Toolkit.get()};
        mTabs.RegisterTab(GetTabId(*Toolkit), Core::FilePath::ToUtf8(Path.stem()), Toolkit->GetTab(), [this, Opened](float DeltaTime) {
            Opened->Tick(DeltaTime);
            mWorkspace.BeginMajorTab();
            Opened->Draw(DeltaTime);
            mWorkspace.EndMajorTab();
        });
        mEditors.push_back(std::move(Toolkit));
    } else if (!Error.empty()) {
        mContext->AddLog(Error);
    }
}
