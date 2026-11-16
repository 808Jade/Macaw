/**
 * @file FConsoleWidget.cpp
 * @brief 콘솔 로그와 명령 입력을 표시하는 에디터 패널입니다.
 * @ingroup Editor
 */

#include "Core/Types.h"
#include "Editor/Widget/FConsoleWidget.h"
#include "Editor/FEditorContext.h"

#include <imgui.h>
#include <imterm/terminal_helpers.hpp>

#include <algorithm>

namespace Editor {

    /**
     * @brief 이 파일에서 사용하는 내부 보조 기능과 상태를 묶습니다.
     */
    namespace {
        /**
         * @brief ImTerm 터미널을 에디터 로그 출력에 연결하는 출력 도우미입니다.
         */
        class FImTermOutputHelper final : public ImTerm::basic_terminal_helper<FImTermOutputHelper, void> {
        };

        /**
         * @brief 현재 테마의 콘솔 강조 색상을 반환합니다.
         * @return 현재 테마의 콘솔 강조 색상입니다.
         */
        ImVec4 GetConsoleAccent() {
            const ImVec4& Link{ImGui::GetStyleColorVec4(ImGuiCol_TextLink)};
            const ImVec4& Text{ImGui::GetStyleColorVec4(ImGuiCol_Text)};

            return {0.6f * Link.x + 0.4f * Text.x, 0.6f * Link.y + 0.4f * Text.y, 0.6f * Link.z + 0.4f * Text.z, Text.w};
        }
    }

    /**
     * @brief 편집 로그를 ImTerm 출력·필터에 동기화합니다.
     */
    class Editor::FConsoleWidget::FOutput {
    public:
        /**
         * @brief 콘솔 로그를 그릴 ImTerm 터미널과 필터 상태를 준비합니다.
         */
        FOutput();

    public:
        /**
         * @brief 로그와 선택적 필터를 예약된 UI 높이에 맞춰 그립니다.
         * @param[in] Context 로그 원본인 편집 컨텍스트입니다.
         * @param[in] ReservedHeight 입력·후보 목록에 남겨 둘 높이입니다.
         * @param[in] ShowFilter 콘솔 필터 입력란의 표시 여부입니다.
         */
        void Draw(const Editor::FEditorContext& Context, float ReservedHeight, bool ShowFilter);

    private:
        /**
         * @brief 편집 컨텍스트의 로그 변경분을 콘솔 터미널에 반영합니다.
         * @param[in] Context 로그 원본인 편집 컨텍스트입니다.
         */
        void SyncLogs(const Editor::FEditorContext& Context);

    private:
        /**
         * @brief 로그를 필터링하고 그리는 ImTerm 터미널입니다.
         */
        ImTerm::terminal<FImTermOutputHelper> mTerminal{};

        /**
         * @brief 터미널 로그 원본 컨텍스트의 비소유 주소입니다.
         */
        const Editor::FEditorContext* mLogContext{};

        /**
         * @brief 마지막으로 반영한 로그 리비전입니다.
         */
        Uint64 mLastLogRevision{};

        /**
         * @brief 터미널에 반영한 원본 로그 항목 수입니다.
         */
        std::size_t mLogCount{};
    };

    /**
     * @brief 콘솔 로그를 그릴 ImTerm 터미널과 필터 상태를 준비합니다.
     */
    Editor::FConsoleWidget::FOutput::FOutput() {
        mTerminal.filter_hint() = "Filter output";
    }

    /**
     * @brief 편집 로그와 필터를 예약된 UI 높이에 맞춰 그립니다.
     */
    void Editor::FConsoleWidget::FOutput::Draw(const Editor::FEditorContext& Context, float ReservedHeight, bool ShowFilter) {
        SyncLogs(Context);

        const ImVec4& Background{ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg)};
        const ImVec4 Accent{GetConsoleAccent()};
        ImTerm::theme& Theme{mTerminal.theme()};
        Theme.message_panel = ImTerm::theme::constexpr_color{Background.x, Background.y, Background.z, Background.w};
        Theme.log_level_colors[ImTerm::message::severity::debug] = ImTerm::theme::constexpr_color{Accent.x, Accent.y, Accent.z, Accent.w};
        Theme.matching_text = Theme.log_level_colors[ImTerm::message::severity::debug];

        mTerminal.DrawOutput(ReservedHeight, ShowFilter);
    }

    /**
     * @brief 편집 컨텍스트의 로그 변경분을 콘솔 터미널에 반영합니다.
     */
    void Editor::FConsoleWidget::FOutput::SyncLogs(const Editor::FEditorContext& Context) {
        const Uint64 Revision{Context.GetLogRevision()};

        if (mLogContext == &Context && Revision == mLastLogRevision) {
            return;
        }

        const Core::TArray<Core::FString>& Logs{Context.GetLogs()};

        if (mLogContext != &Context || Logs.size() < mLogCount || Revision - mLastLogRevision != Logs.size() - mLogCount) {
            mTerminal.clear();
            mLogCount = 0;
        }

        for (; mLogCount < Logs.size(); ++mLogCount) {
            const Core::FString& Message{Logs[mLogCount]};
            const bool IsCommand{Message.starts_with("> ")};
            const ImTerm::message::severity::severity_t Severity{IsCommand ? ImTerm::message::severity::debug : ImTerm::message::severity::info};

            mTerminal.add_message({Severity, Message, 0, IsCommand ? Message.size() : std::size_t{}, false});
        }

        mLogContext = &Context;
        mLastLogRevision = Revision;
    }

    /**
     * @brief FConsoleWidget의 초기 상태와 전달받은 의존성을 구성합니다.
     */
    Editor::FConsoleWidget::FConsoleWidget(Editor::FEditorContext* Context)
		: Editor::FEditorContextWidget{Context, "Console"},
		  mOutput{std::make_unique<Editor::FConsoleWidget::FOutput>()},
		  mCommandInput{Context} {
    }

    /**
     * @brief 객체의 수명을 끝내고 소유한 리소스를 정리합니다.
     */
    Editor::FConsoleWidget::~FConsoleWidget() = default;

    /**
     * @brief 콘솔 패널에 메시지를 추가하고 스크롤 갱신을 요청합니다.
     */
    void Editor::FConsoleWidget::AddLog(const Core::FString& Message) {
        GetEditorContext()->AddLog(Message);
    }

    /**
     * @brief 패널 고유의 ImGui 위젯과 내용을 그립니다.
     */
    void Editor::FConsoleWidget::DrawContent(float) {
        const float InputHeight{ImGui::GetFrameHeightWithSpacing()};
        const ImGuiStyle& Style{ImGui::GetStyle()};
        const ImVec2 Available{ImGui::GetContentRegionAvail()};
        const bool ShowFilter{Available.y >= 4.0f * InputHeight && Available.x >= 10.0f * ImGui::GetFontSize()};
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, std::max(Style.ChildRounding, Style.FrameRounding));
        mOutput->Draw(*GetEditorContext(), InputHeight, ShowFilter);
        ImGui::PopStyleVar();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(GetConsoleAccent(), ">");
        ImGui::SameLine(0.0f, Style.ItemInnerSpacing.x);
        mCommandInput.Draw("OutputCommand", ImGui::GetContentRegionAvail().x, true);
    }

}
