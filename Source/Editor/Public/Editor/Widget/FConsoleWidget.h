/**
 * @file FConsoleWidget.h
 * @brief 로그 출력과 콘솔 명령 입력을 함께 표시하는 에디터 위젯입니다.
 * @ingroup Editor
 */

#pragma once

#include "Editor/Widget/FEditorContextWidget.h"
#include "Editor/FConsoleCommandInput.h"
#include <memory>

namespace Editor {
    /**
     * @brief 로그 출력과 콘솔 명령 입력을 함께 표시하는 에디터 위젯입니다.
     */
    class FConsoleWidget final : public Editor::FEditorContextWidget {
    private:
        class FOutput;

    public:
        /**
         * @brief 로그 출력 상태와 콘솔 명령 입력을 구성합니다.
         * @param[in] Context 편집 대상과 공유 서비스를 제공하는 에디터 컨텍스트의 비소유 주소입니다.
         */
        explicit FConsoleWidget(Editor::FEditorContext* Context);

        /**
         * @brief 소유한 콘솔 출력과 입력 상태를 정리합니다.
         */
        ~FConsoleWidget() override;

    public:
        /**
         * @brief 에디터 컨텍스트의 콘솔 로그에 메시지를 추가합니다.
         * @param[in] Message 콘솔 출력에 추가할 로그 문자열입니다.
         */
        void AddLog(const Core::FString& Message);

    private:
        /**
         * @brief 로그 출력·필터와 콘솔 명령 입력을 그립니다.
         * @param[in] DeltaTime 이전 프레임 이후 경과한 시간이며 단위는 초입니다.
         */
        void DrawContent(float DeltaTime) override;

    private:
        /**
         * @brief ImTerm 콘솔 출력 상태의 소유 객체입니다.
         */
        std::unique_ptr<Editor::FConsoleWidget::FOutput> mOutput{};

        /**
         * @brief 콘솔 명령 실행·완성·기록을 제공하는 입력 위젯입니다.
         */
        Editor::FConsoleCommandInput mCommandInput;
    };
}
