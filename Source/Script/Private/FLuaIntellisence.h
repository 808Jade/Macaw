/**
 * @file FLuaIntellisence.h
 * @brief 텍스트 편집기에 Lua 자동 완성과 진단을 제공하는 인텔리센스를 선언합니다.
 * @ingroup Script
 */

#pragma once

#include "FLuaLanguageServer.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <rapidjson/fwd.h>
#include <chrono>
#include <cstdint>

struct lua_State;

namespace Script {
    /**
     * @brief 편집 중인 Lua 문서를 lua-language-server와 동기화하고 자동 완성과 진단을 편집기에 표시합니다.
     * @ingroup Script
     * @note 서버를 사용할 수 없을 때에도 내장 Lua 파서로 구문 오류를 표시하고 키워드·엔진 바인딩 이름으로 자동 완성합니다.
     */
    class FLuaIntellisence {
    public:
        /**
         * @brief 편집기에 자동 완성 콜백을 등록합니다.
         * @param[in,out] Editor 자동 완성과 진단을 표시할 편집기이며 이 객체보다 오래 유지되어야 합니다.
         */
        explicit FLuaIntellisence(TextEditor& Editor);

        /**
         * @brief 편집기의 자동 완성 설정을 해제하고 구문 검사용 Lua 상태를 닫습니다.
         */
        ~FLuaIntellisence();

    public:
        /**
         * @brief 기존 문서를 닫고 새 문서를 열며, 작업 공간이 바뀌었거나 서버가 멈춰 있으면 서버를 다시 시작합니다.
         * @param[in] Path 편집할 스크립트 파일 경로이며 LSP 문서 URI로 사용합니다.
         * @param[in] Workspace 서버의 작업 공간 루트이며 상위 폴더의 Saved/LuaLanguageServer에 서버 캐시를 둡니다.
         * @param[in] Source LF 줄 끝으로 정규화한 문서 내용입니다.
         * @note 서버 시작에 실패해도 내장 구문 검사와 기본 자동 완성은 동작합니다.
         */
        void Open(const std::filesystem::path& Path, const std::filesystem::path& Workspace, std::string Source);

        /**
         * @brief 진행 중인 자동 완성을 취소하고 서버에 문서 닫기를 알린 뒤 표시한 진단을 지웁니다.
         * @note 서버 프로세스는 다음 Open()에서 재사용할 수 있도록 유지합니다.
         */
        void Close();

        /**
         * @brief 편집된 문서 내용을 반영하고 잠시 뒤 구문 검사와 서버 동기화를 예약합니다.
         * @param[in] Source LF 줄 끝으로 정규화한 문서 전체 내용입니다.
         */
        void Edit(std::string Source);

        /**
         * @brief 최신 내용을 서버에 동기화하고 문서 저장을 알립니다.
         */
        void Save();

        /**
         * @brief 서버 메시지를 처리하고 예약 시각이 지났으면 구문 검사와 문서 동기화를 수행합니다.
         * @note 매 프레임 편집기를 그리기 전에 호출해야 합니다.
         */
        void Update();

    private:
        /**
         * @brief 편집기의 자동 완성 요청을 서버에 보내거나 서버가 없으면 기본 후보로 채웁니다.
         * @param[in,out] State 검색어를 읽고 후보 목록 또는 비동기 응답 약속을 기록할 자동 완성 상태입니다.
         */
        void Complete(TextEditor::AutoCompleteState& State);

        /**
         * @brief 응답을 기다리는 자동 완성 요청이 있으면 서버에 취소를 알립니다.
         */
        void CancelCompletion();

        /**
         * @brief 받은 JSON-RPC 메시지를 서버 요청, 진단 알림 또는 우리 요청의 응답으로 분류해 처리합니다.
         * @param[in] Message 파싱한 JSON-RPC 메시지입니다.
         */
        void HandleMessage(const rapidjson::Value& Message);

        /**
         * @brief 서버가 보낸 요청에 응답하며 workspace/configuration에는 엔진용 Lua 설정을 돌려줍니다.
         * @param[in] Message id와 method를 가진 서버 요청 메시지입니다.
         */
        void HandleRequest(const rapidjson::Value& Message);

        /**
         * @brief 자동 완성 응답에서 단순 텍스트 삽입 후보만 골라 편집기에 전달합니다.
         * @param[in] Result textDocument/completion 응답의 result 값입니다.
         * @note 스니펫이나 추가 편집이 필요한 후보는 편집기가 적용할 수 없으므로 제외합니다.
         */
        void ApplyCompletions(const rapidjson::Value& Result);

        /**
         * @brief 현재 문서 버전에 대한 오류·경고 진단을 물결 밑줄과 줄 표시로 그립니다.
         * @param[in] Parameters textDocument/publishDiagnostics 알림의 params 값입니다.
         * @note 다른 문서나 이전 버전의 진단은 무시합니다.
         */
        void ApplyDiagnostics(const rapidjson::Value& Parameters);

        /**
         * @brief 내장 Lua 파서로 현재 내용을 컴파일해 구문 오류 위치를 표시합니다.
         */
        void CheckSyntax();

        /**
         * @brief 서버에 보낸 버전이 현재와 다르면 문서 열기 또는 전체 내용 변경을 알립니다.
         */
        void SyncDocument();

        /**
         * @brief 응답이 필요 없는 JSON-RPC 알림을 서버에 보냅니다.
         * @param[in] Method 알림 메서드 이름입니다.
         * @param[in] Parameters 이미 직렬화한 params JSON입니다.
         */
        void Notify(std::string_view Method, std::string Parameters);

    private:
        /**
         * @brief 자동 완성과 진단을 표시할 편집기입니다.
         */
        TextEditor& mEditor;

        /**
         * @brief LSP 메시지를 주고받는 lua-language-server 연결입니다.
         */
        FLuaLanguageServer mServer{};

        /**
         * @brief 현재 서버를 시작한 작업 공간 루트입니다.
         */
        std::filesystem::path mWorkspace{};

        /**
         * @brief 서버에 라이브러리로 등록할 엔진 바인딩 선언 파일(Macaw.d.lua)의 경로입니다.
         */
        std::string mLibraryPath{};

        /**
         * @brief 열린 문서의 file URI이며 비어 있으면 열린 문서가 없습니다.
         */
        std::string mUri{};

        /**
         * @brief 열린 문서의 최신 내용입니다.
         */
        std::string mSource{};

        /**
         * @brief 구문 검사와 서버 동기화를 수행할 예약 시각입니다.
         */
        std::chrono::steady_clock::time_point mCheckAt{};

        /**
         * @brief 마지막 자동 완성 요청을 보낸 커서 위치이며 응답이 아직 유효한지 판단합니다.
         */
        TextEditor::DocPos mCompletionPosition{};

        /**
         * @brief 구문 검사에 재사용하는 Lua 상태이며 처음 검사할 때 만듭니다.
         */
        lua_State* mSyntaxState{};

        /**
         * @brief 편집할 때마다 증가하는 문서 버전입니다.
         */
        std::uint64_t mVersion{};

        /**
         * @brief 서버에 마지막으로 보낸 문서 버전이며 0이면 서버에 문서를 열지 않았습니다.
         */
        std::uint64_t mSentVersion{};

        /**
         * @brief 진단을 마지막으로 반영한 문서 버전입니다.
         */
        std::uint64_t mCheckedVersion{};

        /**
         * @brief 표시 중인 진단이 속한 문서 버전이며 다르면 오래된 진단을 지웁니다.
         */
        std::uint64_t mDisplayedVersion{};

        /**
         * @brief 마지막으로 발급한 JSON-RPC 요청 id입니다.
         */
        std::uint64_t mNextRequest{};

        /**
         * @brief 응답을 기다리는 initialize 요청 id이며 0이면 없습니다.
         */
        std::uint64_t mInitializeRequest{};

        /**
         * @brief 응답을 기다리는 자동 완성 요청 id이며 0이면 없습니다.
         */
        std::uint64_t mCompletionRequest{};

        /**
         * @brief 마지막 자동 완성 요청을 보낸 시점의 문서 버전입니다.
         */
        std::uint64_t mCompletionVersion{};

        /**
         * @brief 서버 초기화가 끝나 문서 동기화와 자동 완성 요청을 보낼 수 있는지 나타냅니다.
         */
        bool mReady{};
    };
}
