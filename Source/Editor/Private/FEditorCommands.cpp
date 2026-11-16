/**
 * @file FEditorCommands.cpp
 * @brief 레벨·프로젝트 전환과 저장·실행·내보내기·종료 명령을 조정합니다.
 * @ingroup Editor
 */

#include "FEditorCommands.h"
#include "FEditorViewportLayout.h"
#include "Editor/FScriptBuildService.h"
#include "Editor/FEditorContext.h"
#include "Editor/FEditorViewport.h"
#include "Editor/FPlayInEditorSession.h"
#include "Editor/FGameExporter.h"
#include "Editor/FProjectDialog.h"
#include "Core/FilePath.h"
#include "Engine/FProject.h"
#include "Platform/Window.h"
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <utility>

using namespace Editor;

FEditorCommands::FEditorCommands(FEditorContext* Context, FPlayInEditorSession* PlaySession, FEditorViewportLayout& ViewportLayout, Platform::FWindowsApplication* Platform, FScriptBuildService& ScriptBuilds)
	: mContext{Context},
	  mPlaySession{PlaySession},
	  mViewportLayout{ViewportLayout},
	  mPlatform{Platform},
	  mScriptBuilds{ScriptBuilds} {
}

FEditorCommands::~FEditorCommands() = default;

void FEditorCommands::SetContentChanged(std::function<void()> Handler) {
    mContentChanged = std::move(Handler);
}

void FEditorCommands::SetCanCloseAssets(std::function<bool()> Handler) {
    mCanCloseAssets = std::move(Handler);
}

bool FEditorCommands::IsPlayPending() const {
    return mPlayPending;
}

void FEditorCommands::CancelPendingPlay() {
    mPlayPending = false;
}

bool FEditorCommands::WantsExit() const {
    return mExit;
}

void FEditorCommands::RequestExit() {
    mExit = true;
}

std::filesystem::path FEditorCommands::ChooseFile(bool Save) {
    std::array<wchar_t, 4096> Buffer{};

    if (Save) {
        const std::wstring Filename{mContext->GetScenePath().empty() ? L"Scene.masset" : mContext->GetScenePath().filename().wstring()};

        std::copy_n(Filename.begin(), std::min(Filename.size(), Buffer.size() - 1), Buffer.begin());
    }

    const std::filesystem::path Directory{mContext->GetProject()->GetAssets()->GetContentRoot() / "Maps"};

    std::error_code Error{};

    std::filesystem::create_directories(Directory, Error);

    OPENFILENAMEW Dialog{};
    Dialog.lStructSize = sizeof(Dialog);
    Dialog.hwndOwner = mPlatform->GetWindowHandle();
    Dialog.lpstrFilter = L"Macaw Scene (*.masset)\0*.masset\0";
    Dialog.lpstrFile = Buffer.data();
    Dialog.nMaxFile = static_cast<DWORD>(Buffer.size());
    Dialog.lpstrInitialDir = Directory.c_str();
    Dialog.lpstrDefExt = L"masset";
    Dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | (Save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    if (Save ? GetSaveFileNameW(&Dialog) : GetOpenFileNameW(&Dialog)) {
        return std::filesystem::path{Buffer.data()};
    }

    return {};
}

void FEditorCommands::SaveLevel(bool SaveAs) {
    const auto Path{SaveAs || mContext->GetScenePath().empty() ? ChooseFile(true) : mContext->GetScenePath()};

    if (!Path.empty()) {
        mContext->SaveScene(Path);
        if (mContentChanged) {
            mContentChanged();
        }
    }
}

void FEditorCommands::OpenLevel() {
    const auto Path{ChooseFile(false)};

    if (!Path.empty() && ConfirmSceneChange()) {
        mContext->LoadScene(Path);
    }
}

void FEditorCommands::RequestPlay() {
    if (mPlaySession->IsActive()) {
        return;
    }

    mScriptBuilds.BuildIfChanged();
    mPlayPending = true;

    UpdatePendingPlay();
}

void FEditorCommands::UpdatePendingPlay() {
    if (!mPlayPending || mScriptBuilds.IsBuilding()) {
        return;
    }

    mPlayPending = false;

    if (mScriptBuilds.HasCompileErrors()) {
        mContext->AddLog("PIE canceled: fix the C# compile errors first");
    } else if (mViewportLayout.CanChangeLayout()) {
        mPlaySession->Start(&mPlatform->GetInput());
    }
}

void FEditorCommands::ExportGame() {
    const auto Destination{Editor::FProjectDialog::ChooseDirectory(mPlatform->GetWindowHandle())};

    if (Destination.empty()) {
        return;
    }

    if (!ConfirmSceneChange()) {
        return;
    }

    std::array<wchar_t, 32768> Executable{};
    const DWORD Length{GetModuleFileNameW(nullptr, Executable.data(), static_cast<DWORD>(Executable.size()))};
    Core::FString Error{};
    bool Exported{};

    if (Length == 0 || Length >= Executable.size()) {
        Error = "Cannot locate the game executable";
    } else {
        const auto Runtime{std::filesystem::path{Executable.data()}.parent_path() / "GameRuntime"};

        Exported = Editor::FGameExporter::Export(*mContext->GetProject(), Runtime, Destination, &mScriptBuilds, Error);
    }

    const Core::FString Message{Exported ? "Exported game to " + Core::FilePath::ToUtf8(Destination) : Error};

    mContext->AddLog(Message);
    MessageBoxW(mPlatform->GetWindowHandle(), Core::FilePath::FromUtf8(Message).c_str(), Exported ? L"Game Exported" : L"Cannot Export Game", MB_OK | (Exported ? MB_ICONINFORMATION : MB_ICONERROR));
}

bool FEditorCommands::ConfirmSceneChange() {
    if (mCanCloseAssets && !mCanCloseAssets()) {
        return false;
    }

    if (!mContext->IsSceneDirty()) {
        return true;
    }

    const Int32 Choice{MessageBoxW(mPlatform->GetWindowHandle(), L"Save changes to the current scene?", L"Scene", MB_YESNOCANCEL | MB_ICONQUESTION)};

    if (Choice == IDCANCEL) {
        return false;
    }

    if (Choice == IDNO) {
        return true;
    }

    const auto Path{mContext->GetScenePath().empty() ? ChooseFile(true) : mContext->GetScenePath()};
    const bool Saved{!Path.empty() && mContext->SaveScene(Path)};

    if (Saved && mContentChanged) {
        mContentChanged();
    }

    return Saved;
}

void FEditorCommands::ChooseProject(bool Create) {
    if (!ConfirmSceneChange()) {
        return;
    }

    mContext->RequestProject({}, Create);
}
