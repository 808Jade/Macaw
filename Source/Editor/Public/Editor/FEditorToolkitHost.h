/**
 * @file FEditorToolkitHost.h
 * @brief 주 탭 내부의 도킹 공간과 위젯 탭·도구 모음·배치 생성을 관리합니다.
 * @ingroup Editor
 */

#pragma once

#include "Editor/FEditorTabManager.h"

#include <functional>

namespace Editor {

    /**
     * @brief 주 탭 내부의 도킹 공간과 위젯 탭·도구 모음·배치 생성을 관리합니다.
     */
    class FEditorToolkitHost {
    public:
        /**
         * @brief FEditorToolkitHost의 초기 상태와 전달받은 의존성을 구성합니다.
         * @param[in] Owner 연결할 상위 주 탭입니다.
         */
        explicit FEditorToolkitHost(Editor::FEditorTab& Owner);

        /**
         * @brief 객체를 소멸하고 소유 멤버와 연결 상태를 정리합니다.
         */
        ~FEditorToolkitHost();

        /**
         * @brief 중복된 수명·연결 상태를 방지하기 위해 복사 생성을 금지합니다.
         */
        FEditorToolkitHost(const Editor::FEditorToolkitHost&) = delete;

        /**
         * @brief 중복된 수명·연결 상태를 방지하기 위해 대입을 금지합니다.
         */
        Editor::FEditorToolkitHost& operator=(const Editor::FEditorToolkitHost&) = delete;

    public:
        /**
         * @brief 툴킷 내부 탭 관리자를 반환합니다.
         * @return 호스트가 소유하는 탭 관리자 참조입니다.
         */
        Editor::FEditorTabManager& GetTabManager();

        /**
         * @brief 툴킷 내부 도킹 공간 식별자를 반환합니다.
         * @return 최근 그리기에서 계산한 ImGui 도킹 공간 식별자입니다.
         */
        Uint32 GetDockSpaceId() const;

        /**
         * @brief 관리하는 탭 또는 내부 창에 입력 포커스가 있는지 검사합니다.
         * @return 해당 편집기 영역에 포커스가 있으면 true입니다.
         */
        bool IsFocused() const;

        /**
         * @brief 도킹 공간 식별자 생성에 사용할 이름을 지정합니다.
         * @param[in] Name 표시 제목과 고유 ImGui 식별자를 담는 이름입니다.
         */
        void SetDockSpaceName(Core::FString Name);

        /**
         * @brief 도킹 콘텐츠를 그리기 전에 호출할 프레임 콜백을 지정합니다.
         * @param[in] Handler 도킹 공간의 콘텐츠보다 먼저 실행할 콜백입니다.
         */
        void SetFrameHandler(std::function<void()> Handler);

        /**
         * @brief 툴킷 도킹 공간 위에 표시할 도구 모음 콜백을 지정합니다.
         * @param[in] Handler 도킹 공간 위에 도구 모음을 그릴 콜백입니다.
         */
        void SetToolbar(std::function<void()> Handler);

        /**
         * @brief 도킹 배치를 처음 만들거나 초기화할 때 호출할 빌더를 지정합니다.
         * @param[in] Handler 도킹 공간 식별자를 받아 초기 배치를 구성할 콜백입니다.
         */
        void SetLayoutBuilder(std::function<void(Uint32)> Handler);

        /**
         * @brief 다음 도킹 공간 그리기에서 배치를 다시 만들도록 요청합니다.
         */
        void RequestLayoutReset();

        /**
         * @brief 도구 모음과 내부 도킹 공간을 그리고 숨겨진 공간의 수명을 유지합니다.
         * @param[in] Visible 창·위젯 또는 도킹 공간의 표시 여부입니다.
         */
        void DrawDockSpace(bool Visible);

        /**
         * @brief 툴킷에 등록된 위젯 탭의 표시 메뉴를 그립니다.
         */
        void DrawWindowMenu();

        /**
         * @brief 소유 위젯 또는 콘텐츠 콜백을 툴킷의 내부 탭으로 등록합니다.
         * @param[in] Id 등록 또는 그리기 항목을 구분할 식별자입니다.
         * @param[in] Label 메뉴 또는 컨트롤에 표시할 레이블입니다.
         * @param[in] Widget 연결하거나 등록할 콘텐츠 위젯입니다.
         * @param[in] Region 기본 배치에 사용할 도킹 영역입니다.
         */
        void AddWidget(Core::FString Id, Core::FString Label, std::unique_ptr<Editor::FEditorWidget> Widget, Editor::EEditorDockRegion Region = Editor::EEditorDockRegion::Center);

        /**
         * @brief 소유 위젯 또는 콘텐츠 콜백을 툴킷의 내부 탭으로 등록합니다.
         * @param[in] Id 등록 또는 그리기 항목을 구분할 식별자입니다.
         * @param[in] Label 메뉴 또는 컨트롤에 표시할 레이블입니다.
         * @param[in] Content 현재 프레임의 콘텐츠를 그릴 콜백입니다.
         * @param[in] Region 기본 배치에 사용할 도킹 영역입니다.
         * @param[in] Closable 사용자가 탭을 닫을 수 있는지 지정합니다.
         * @param[in] WindowName 탭 콘텐츠에 사용할 고유 ImGui 창 이름입니다.
         */
        void AddWidget(Core::FString Id, Core::FString Label, std::function<void(float)> Content, Editor::EEditorDockRegion Region = Editor::EEditorDockRegion::Center, bool Closable = true, Core::FString WindowName = {});

    private:
        /**
         * @brief 등록된 패널 영역에 맞춰 툴킷 기본 분할 배치를 만듭니다.
         * @param[in] DockSpace 배치를 구성할 ImGui 도킹 공간 식별자입니다.
         */
        void BuildDefaultLayout(Uint32 DockSpace);

    private:
        /**
         * @brief 툴킷 호스트를 소유하는 주 탭 참조입니다.
         */
        Editor::FEditorTab& mOwner;

        /**
         * @brief 위젯·주 탭을 등록하고 표시하는 탭 관리자입니다.
         */
        Editor::FEditorTabManager mTabs{};

        /**
         * @brief 내부 도킹 공간 식별자 생성에 사용하는 이름입니다.
         */
        Core::FString mDockSpaceName{"ToolkitDockSpace"};

        /**
         * @brief 툴킷 내부 도킹 공간의 ImGui 식별자입니다.
         */
        Uint32 mDockSpace{};

        /**
         * @brief 다음 프레임에 내부 도킹 배치를 다시 만들지 나타냅니다.
         */
        bool mResetLayout{};

        /**
         * @brief 도킹 공간을 그리기 전에 실행하는 프레임 콜백입니다.
         */
        std::function<void()> mFrameHandler{};

        /**
         * @brief 내부 도킹 공간 위에 도구 모음을 그리는 콜백입니다.
         */
        std::function<void()> mToolbar{};

        /**
         * @brief 내부 도킹 공간의 초기 배치를 구성하는 콜백입니다.
         */
        std::function<void(Uint32)> mLayoutBuilder{};
    };
}
