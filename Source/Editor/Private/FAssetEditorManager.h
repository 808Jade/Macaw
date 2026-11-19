/**
 * @file FAssetEditorManager.h
 * @brief 에셋 종류별 편집기를 열고 활성 탭과 편집기 수명을 관리합니다.
 * @ingroup Editor
 */

#pragma once

#include "Core/Types.h"
#include "Asset/FAssetFile.h"
#include "Editor/FEditorTabManager.h"
#include "Editor/Widget/FEditorContentWidget.h"
#include "Script/FIDETab.h"
#include <filesystem>
#include <functional>
#include <memory>

namespace Platform {
    class FWindowsApplication;
}

namespace Editor {
    class FAssetEditorToolkit;
    class FEditorContext;
    class FEditorAssetCatalog;
    class FAssetThumbnailCache;
    class FEditorWorkspace;
    class FEditorCommands;
    class FEditorToolkitHost;
}

namespace Editor {
    /**
     * @brief 에셋 종류별 편집기를 열고 활성 탭과 편집기 수명을 관리합니다.
     */
    class FAssetEditorManager {
    public:
        /**
         * @brief FAssetEditorManager의 초기 상태와 전달받은 의존성을 구성합니다.
         * @param[in] Context 편집 대상과 공유 서비스를 제공하는 에디터 컨텍스트의 비소유 주소입니다.
         * @param[in] Platform 창·입력·파일 대화상자를 제공하는 플랫폼 애플리케이션입니다.
         * @param[in] Catalog 에셋 검색과 메타데이터를 제공하는 카탈로그입니다.
         * @param[in] Thumbnails 에셋 썸네일과 애니메이션 미리 보기 캐시입니다.
         * @param[in] Workspace 주 탭·레벨 패널의 도킹 배치와 포커스 서비스입니다.
         * @param[in] Commands 레벨·프로젝트·실행 명령 서비스입니다.
         */
        FAssetEditorManager(Editor::FEditorContext* Context, Platform::FWindowsApplication* Platform, Editor::FEditorAssetCatalog& Catalog, Editor::FAssetThumbnailCache& Thumbnails, Editor::FEditorWorkspace& Workspace, Editor::FEditorCommands& Commands);

        /**
         * @brief 객체를 소멸하고 소유 멤버와 연결 상태를 정리합니다.
         */
        ~FAssetEditorManager();

    public:
        /**
         * @brief 에셋 종류에 맞는 편집기를 열거나 기존 탭에 포커스를 요청합니다.
         * @param[in] Path 대상 에셋 또는 콘텐츠 경로입니다.
         * @param[in] Type 편집할 에셋의 종류입니다.
         */
        void OpenContentAsset(const std::filesystem::path& Path, Asset::EAssetType Type);

        /**
         * @brief Lua 스크립트를 스크립트 IDE 탭에 열고 탭을 표시합니다.
         * @param[in] Path 프로젝트 에셋 기준으로 해석할 스크립트 경로입니다.
         * @note 같은 파일을 수정 중이면 다시 읽지 않고 탭만 표시합니다. 다른 파일에 저장하지 않은 변경이 있으면 저장 여부를 먼저 묻고 취소하면 열지 않습니다.
         */
        void OpenLuaScript(const std::filesystem::path& Path);

        /**
         * @brief 열린 에셋 탭을 그리고 활성 편집기를 추적하며 닫힌 편집기를 제거합니다.
         * @param[in] DeltaTime 이전 프레임 이후 경과한 시간이며 단위는 초입니다.
         */
        void Draw(float DeltaTime);

        /**
         * @brief 현재 활성 에셋 편집기의 툴킷 호스트를 조회합니다.
         * @return 활성 호스트의 비소유 주소이며 없으면 nullptr입니다.
         */
        Editor::FEditorToolkitHost* GetActiveToolkitHost() const;

        /**
         * @brief 활성 에셋을 콘텐츠 브라우저에서 찾아 표시합니다.
         */
        void BrowseActiveAsset();

        /**
         * @brief 관리하는 탭 또는 내부 창에 입력 포커스가 있는지 검사합니다.
         * @return 해당 편집기 영역에 포커스가 있으면 true입니다.
         */
        bool IsFocused() const;

        /**
         * @brief Lua 스크립트 편집기 탭이 표시 중이며 입력 포커스를 가졌는지 검사합니다.
         * @return 스크립트 편집기 탭에 포커스가 있으면 true입니다.
         */
        bool IsScriptEditorFocused() const;

        /**
         * @brief 편집기를 닫아도 되는지 확인하며 필요한 경우 변경 내용의 처리를 요청합니다.
         * @return 닫기를 허용하면 true입니다.
         */
        bool CanClose();

        /**
         * @brief 폐기된 에셋 경로를 참조하는 편집기 콘텐츠를 닫거나 해제합니다.
         * @param[in] Absolute 폐기 또는 경로 작업 대상의 절대 경로입니다.
         */
        void CloseRetired(const std::filesystem::path& Absolute);

        /**
         * @brief 열린 머티리얼 중 경로 조건을 만족하는 항목이 있는지 검사합니다.
         * @param[in] Predicate 열린 머티리얼 경로를 판정할 조건입니다.
         * @return 조건에 맞는 열린 머티리얼이 있으면 true입니다.
         */
        bool HasOpenMaterial(const std::function<bool(const std::filesystem::path&)>& Predicate) const;

        /**
         * @brief 스크립트 IDE에 조건에 맞는 저장하지 않은 스크립트가 열려 있는지 검사합니다.
         * @param[in] Predicate 열린 스크립트의 절대 경로를 판정할 조건입니다.
         * @return 스크립트 탭이 표시 중이고 변경 내용이 있으며 경로가 조건에 맞으면 true입니다.
         */
        bool HasUnsavedScript(const std::function<bool(const std::filesystem::path&)>& Predicate) const;

        /**
         * @brief 변경이 발생했을 때 호출할 알림 콜백을 지정합니다.
         * @param[in] Handler 변경 내용을 알릴 콜백입니다.
         */
        void SetChangedHandler(std::function<void()> Handler);

    private:
        /**
         * @brief 스크립트 IDE에 저장하지 않은 변경이 있으면 저장, 버리기 또는 취소를 묻습니다.
         * @return 변경이 없거나, 저장에 성공했거나, 버리기를 선택했으면 true입니다. 취소했거나 저장에 실패하면 false입니다.
         * @note 저장에 실패하면 오류를 볼 수 있도록 스크립트 탭을 다시 엽니다.
         */
        bool ConfirmScriptChange();

        /**
         * @brief 포커스와 마지막 활성 상태로 표시 중인 에셋 편집기를 찾습니다.
         * @return 활성 편집기의 비소유 주소이며 없으면 nullptr입니다.
         */
        Editor::FAssetEditorToolkit* FindActiveEditor() const;

        /**
         * @brief 에셋 경로와 종류를 조합해 탭 등록 식별자를 만듭니다.
         * @param[in] Toolkit 탭 식별자를 계산할 에셋 편집기입니다.
         * @return 편집기 에셋을 구분하는 탭 식별자입니다.
         */
        Core::FString GetTabId(const Editor::FAssetEditorToolkit& Toolkit) const;

    private:
        /**
         * @brief 편집 대상과 공유 서비스에 접근하는 에디터 컨텍스트의 비소유 주소입니다.
         */
        Editor::FEditorContext* mContext{};

        /**
         * @brief Win32 창·입력·파일 선택을 제공하는 플랫폼 애플리케이션입니다.
         */
        Platform::FWindowsApplication* mPlatform{};

        /**
         * @brief 공유 에셋 카탈로그의 참조입니다.
         */
        Editor::FEditorAssetCatalog& mCatalog;

        /**
         * @brief 에셋 썸네일과 애니메이션 미리 보기 설정의 공유 캐시입니다.
         */
        Editor::FAssetThumbnailCache& mThumbnails;

        /**
         * @brief 주 탭과 레벨 패널의 도킹 배치·포커스를 관리하는 서비스입니다.
         */
        Editor::FEditorWorkspace& mWorkspace;

        /**
         * @brief 레벨·프로젝트·실행 명령을 처리하는 서비스입니다.
         */
        Editor::FEditorCommands& mCommands;

        /**
         * @brief 위젯·주 탭을 등록하고 표시하는 탭 관리자입니다.
         */
        Editor::FEditorTabManager mTabs{};

        /**
         * @brief Lua 스크립트 하나를 편집하는 IDE 탭 내용입니다.
         */
        Script::FIDETab mScriptIDE{};

        /**
         * @brief 스크립트 탭 안에 mScriptIDE를 그리고 Ctrl+S 저장을 처리하는 내용 위젯입니다.
         */
        Editor::FEditorContentWidget mScriptContent;

        /**
         * @brief 스크립트 IDE를 표시하는 주 탭이며 닫을 때 저장하지 않은 변경을 확인합니다.
         */
        Editor::FEditorTab mScriptTab{"Script IDE###ScriptIDE", true, Editor::EEditorTabRole::Major};

        /**
         * @brief 열려 있는 에셋 편집기 툴킷의 소유 목록입니다.
         */
        Core::TArray<std::unique_ptr<Editor::FAssetEditorToolkit>> mEditors{};

        /**
         * @brief 마지막으로 활성화된 에셋 편집기의 비소유 주소입니다.
         */
        Editor::FAssetEditorToolkit* mActiveEditor{};

        /**
         * @brief 편집 내용 또는 설정 변경을 알리는 콜백입니다.
         */
        std::function<void()> mChanged{};
    };
}
