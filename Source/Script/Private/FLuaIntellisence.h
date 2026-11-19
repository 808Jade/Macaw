#pragma once

#include "FLuaLanguageServer.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <rapidjson/fwd.h>
#include <chrono>
#include <cstdint>

struct lua_State;

namespace Script {
    class FLuaIntellisence {
    public:
        explicit FLuaIntellisence(TextEditor& Editor);
        ~FLuaIntellisence();

    public:
        void Open(const std::filesystem::path& Path, const std::filesystem::path& Workspace, std::string Source);
        void Close();
        void Edit(std::string Source);
        void Save();
        void Update();

    private:
        void Complete(TextEditor::AutoCompleteState& State);
        void CancelCompletion();
        void HandleMessage(const rapidjson::Value& Message);
        void HandleRequest(const rapidjson::Value& Message);
        void ApplyCompletions(const rapidjson::Value& Result);
        void ApplyDiagnostics(const rapidjson::Value& Parameters);
        void CheckSyntax();
        void SyncDocument();
        void Notify(std::string_view Method, std::string Parameters);

    private:
        TextEditor& mEditor;
        FLuaLanguageServer mServer{};
        std::filesystem::path mWorkspace{};
        std::string mLibraryPath{};
        std::string mUri{};
        std::string mSource{};
        std::chrono::steady_clock::time_point mCheckAt{};
        TextEditor::DocPos mCompletionPosition{};
        lua_State* mSyntaxState{};
        std::uint64_t mVersion{};
        std::uint64_t mSentVersion{};
        std::uint64_t mCheckedVersion{};
        std::uint64_t mDisplayedVersion{};
        std::uint64_t mNextRequest{};
        std::uint64_t mInitializeRequest{};
        std::uint64_t mCompletionRequest{};
        std::uint64_t mCompletionVersion{};
        bool mReady{};
    };
}
