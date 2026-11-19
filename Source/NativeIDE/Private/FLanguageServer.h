/**
 * @file FLanguageServer.h
 * @brief 언어 서버 자식 프로세스와 LSP 메시지를 주고받는 연결을 선언합니다.
 * @ingroup NativeIDE
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <Windows.h>

namespace NativeIDE {
    /**
     * @brief 언어 서버 프로세스를 실행하고 표준 입출력 파이프로 LSP 메시지 본문을 주고받습니다.
     * @ingroup NativeIDE
     * @note 읽기와 쓰기는 전용 스레드에서 수행하며 Content-Length 헤더는 이 클래스가 붙이고 제거합니다.
     */
    class FLanguageServer {
    public:
        /**
         * @brief 실행 중이 아닌 상태로 만듭니다.
         */
        FLanguageServer() = default;

        /**
         * @brief 실행 중인 서버 프로세스를 종료하고 스레드와 핸들을 정리합니다.
         */
        ~FLanguageServer();

    public:
        /**
         * @brief 기존 서버를 멈추고 새 서버 프로세스와 입출력 스레드를 시작합니다.
         * @param[in] Executable 실행할 서버 파일 경로입니다.
         * @param[in] Directory 서버의 작업 폴더입니다.
         * @param[in] Arguments 실행 파일 이름 뒤에 붙일 명령줄 인자입니다.
         * @return 프로세스와 스레드를 모두 시작했으면 true입니다.
         * @note 서버의 표준 오류 출력은 버립니다.
         */
        bool Start(const std::filesystem::path& Executable, const std::filesystem::path& Directory, std::wstring_view Arguments);

        /**
         * @brief 서버 프로세스를 강제로 종료하고 스레드, 핸들과 대기 중인 메시지를 정리합니다.
         * @note 실행 중이 아니면 아무것도 하지 않습니다.
         */
        void Stop();

        /**
         * @brief 서버와의 연결이 살아 있는지 검사합니다.
         * @return 프로세스를 시작한 뒤 읽기·쓰기 오류나 Stop()이 없었으면 true입니다.
         */
        bool IsRunning() const;

        /**
         * @brief JSON-RPC 메시지 본문을 전송 대기열에 추가합니다.
         * @param[in] Message 헤더 없이 보낼 JSON 메시지 본문입니다.
         * @note 연결이 끊긴 상태에서는 메시지를 버립니다.
         */
        void Send(std::string Message);

        /**
         * @brief 지금까지 받은 메시지 본문을 꺼내고 수신 대기열을 비웁니다.
         * @return 수신 순서대로 정렬된 JSON 메시지 본문 목록입니다.
         */
        std::vector<std::string> TakeMessages();

    private:
        /**
         * @brief 서버 출력에서 Content-Length 단위로 메시지를 잘라 수신 대기열에 쌓습니다.
         * @note 읽기 스레드에서 실행하며 헤더가 잘못되었거나 파이프가 닫히면 연결을 끊긴 상태로 바꿉니다.
         */
        void Read();

        /**
         * @brief 전송 대기열의 메시지에 Content-Length 헤더를 붙여 서버 입력에 씁니다.
         * @param[in] Stop 스레드 종료 요청을 알리는 토큰입니다.
         * @note 쓰기 스레드에서 실행하며 쓰기에 실패하면 연결을 끊긴 상태로 바꿉니다.
         */
        void Write(std::stop_token Stop);

    private:
        /**
         * @brief 서버 프로세스 핸들입니다.
         */
        HANDLE mProcess{};
        HANDLE mJob{};

        /**
         * @brief 서버 표준 입력에 연결된 파이프의 쓰기 핸들입니다.
         */
        HANDLE mInput{};

        /**
         * @brief 서버 표준 출력에 연결된 파이프의 읽기 핸들입니다.
         */
        HANDLE mOutput{};

        /**
         * @brief 연결이 살아 있는지 나타내며 입출력 스레드와 공유합니다.
         */
        std::atomic_bool mRunning{};

        /**
         * @brief mIncoming과 mOutgoing을 보호합니다.
         */
        std::mutex mMutex{};

        /**
         * @brief 보낼 메시지가 생겼거나 연결이 끊겼음을 쓰기 스레드에 알립니다.
         */
        std::condition_variable_any mQueued{};

        /**
         * @brief 받았지만 아직 TakeMessages()로 꺼내지 않은 메시지 본문입니다.
         */
        std::vector<std::string> mIncoming{};

        /**
         * @brief 쓰기 스레드가 아직 보내지 않은 메시지 본문입니다.
         */
        std::vector<std::string> mOutgoing{};

        /**
         * @brief Read()를 실행하는 스레드입니다.
         */
        std::jthread mReader{};

        /**
         * @brief Write()를 실행하는 스레드입니다.
         */
        std::jthread mWriter{};
    };
}
