#include "FLuaIntellisence.h"
#include "Core/FilePath.h"

#include <lua.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <string_view>
#include <utility>

using namespace Script;

namespace {
    constexpr size_t SyntaxDiagnostic{1};
    constexpr size_t ServerDiagnostic{2};

    std::filesystem::path GetServerDirectory() {
        std::array<wchar_t, 32768> Executable{};
        const DWORD Count{GetModuleFileNameW(nullptr, Executable.data(), static_cast<DWORD>(Executable.size()))};
        return Count > 0 && Count < Executable.size() ? std::filesystem::path{Executable.data()}.parent_path() / L"LuaLanguageServer" : std::filesystem::path{};
    }

    std::string FileUri(const std::filesystem::path& Path) {
        constexpr char Hex[]{"0123456789ABCDEF"};
        const std::string Name{Core::FilePath::ToGenericUtf8(Path)};
        std::string Uri{Name.starts_with("//") ? "file:" : "file:///"};
        for (const unsigned char Byte : Name) {
            if ((Byte >= 'A' && Byte <= 'Z') || (Byte >= 'a' && Byte <= 'z') || (Byte >= '0' && Byte <= '9') || std::string_view{"/-_.~"}.find(Byte) != std::string_view::npos) {
                Uri += static_cast<char>(Byte);
            } else {
                Uri += '%';
                Uri += Hex[Byte >> 4];
                Uri += Hex[Byte & 15];
            }
        }
        return Uri;
    }

    std::string QuoteJson(std::string_view Text) {
        rapidjson::StringBuffer Buffer{};
        rapidjson::Writer<rapidjson::StringBuffer> Writer{Buffer};
        Writer.String(Text.empty() ? "" : Text.data(), static_cast<rapidjson::SizeType>(Text.size()));
        return {Buffer.GetString(), Buffer.GetSize()};
    }

    const rapidjson::Value& Get(const rapidjson::Value& Object, const char* Key) {
        static const rapidjson::Value Null{};
        if (Object.IsObject()) {
            const auto Member{Object.FindMember(Key)};
            if (Member != Object.MemberEnd()) {
                return Member->value;
            }
        }
        return Null;
    }

    std::string_view String(const rapidjson::Value& Value) {
        return Value.IsString() ? std::string_view{Value.GetString(), Value.GetStringLength()} : std::string_view{};
    }

    bool ReadPosition(const rapidjson::Value& Value, TextEditor::DocPos& Position) {
        const auto& Line{Get(Value, "line")};
        const auto& Column{Get(Value, "character")};
        if (!Line.IsUint() || !Column.IsUint()) {
            return false;
        }

        Position = {Line.GetUint(), Column.GetUint()};
        return true;
    }
}

FLuaIntellisence::FLuaIntellisence(TextEditor& Editor)
	: mEditor{Editor} {
    TextEditor::AutoCompleteConfig Config{};
    Config.triggerCharacters = ".:";
    Config.callback = [this](TextEditor::AutoCompleteState& State) {
        Complete(State);
    };
    mEditor.SetAutoCompleteConfig(&Config);
}

FLuaIntellisence::~FLuaIntellisence() {
    mEditor.SetAutoCompleteConfig(nullptr);
    if (mSyntaxState != nullptr) {
        lua_close(mSyntaxState);
    }
}

void FLuaIntellisence::Open(const std::filesystem::path& Path, const std::filesystem::path& Workspace, std::string Source) {
    Close();
    mUri = FileUri(Path);
    Edit(std::move(Source));
    mCheckAt = std::chrono::steady_clock::now();

    if (mServer.IsRunning() && mWorkspace == Workspace) {
        return;
    }

    mWorkspace = Workspace;
    mReady = false;
    mInitializeRequest = 0;
    const auto Directory{GetServerDirectory()};
    if (Directory.empty() || !mServer.Start(Directory, Workspace.parent_path() / L"Saved/LuaLanguageServer")) {
        return;
    }

    mLibraryPath = Core::FilePath::ToGenericUtf8(Directory / L"Macaw.d.lua");
    mInitializeRequest = ++mNextRequest;
    mServer.Send(R"({"jsonrpc":"2.0","method":"initialize","id":)" + std::to_string(mInitializeRequest) + R"(,"params":{"processId":)" + std::to_string(GetCurrentProcessId()) + R"(,"rootUri":)" + QuoteJson(FileUri(Workspace)) + R"(,"capabilities":{"general":{"positionEncodings":["utf-16"]},"workspace":{"configuration":true},"textDocument":{"publishDiagnostics":{"versionSupport":true}}}}})");
}

void FLuaIntellisence::Close() {
    CancelCompletion();
    if (mSentVersion != 0) {
        Notify("textDocument/didClose", R"({"textDocument":{"uri":)" + QuoteJson(mUri) + "}}");
    }

    mUri.clear();
    mSource.clear();
    mSentVersion = 0;
    mEditor.SetAutoCompleteSuggestions({});
    mEditor.ClearMarkers();
    mEditor.ClearSquiggles();
}

void FLuaIntellisence::Edit(std::string Source) {
    CancelCompletion();
    mSource = std::move(Source);
    ++mVersion;
    mCheckAt = std::chrono::steady_clock::now() + std::chrono::milliseconds{200};
}

void FLuaIntellisence::Save() {
    if (mReady && mServer.IsRunning() && !mUri.empty()) {
        SyncDocument();
        Notify("textDocument/didSave", R"({"textDocument":{"uri":)" + QuoteJson(mUri) + "}}");
    }
}

void FLuaIntellisence::Update() {
    if (mDisplayedVersion != mVersion) {
        mEditor.ClearSquiggles();
        mEditor.ClearMarkers();
        mDisplayedVersion = mVersion;
    }

    for (const auto& Packet : mServer.TakeMessages()) {
        rapidjson::Document Message{};
        Message.Parse(Packet.data(), Packet.size());
        if (!Message.HasParseError()) {
            HandleMessage(Message);
        }
    }

    if (!mServer.IsRunning() && mReady) {
        mReady = false;
        mCheckedVersion = 0;
        mEditor.ClearSquiggles(ServerDiagnostic);
        if (mCompletionRequest != 0) {
            mEditor.SetAutoCompleteSuggestions({});
            mCompletionRequest = 0;
        }
    }

    if (!mUri.empty() && std::chrono::steady_clock::now() >= mCheckAt) {
        if (mCheckedVersion != mVersion) {
            CheckSyntax();
        }
        if (mReady) {
            SyncDocument();
        }
    }
}

void FLuaIntellisence::Complete(TextEditor::AutoCompleteState& State) {
    State.suggestions.clear();
    if (mUri.empty()) {
        return;
    }

    if (mReady && mServer.IsRunning()) {
        CancelCompletion();
        SyncDocument();
        mCompletionRequest = ++mNextRequest;
        mCompletionVersion = mVersion;
        mCompletionPosition = mEditor.GetCurrentCursorPosition();
        mServer.Send(R"({"jsonrpc":"2.0","method":"textDocument/completion","id":)" + std::to_string(mCompletionRequest) + R"(,"params":{"textDocument":{"uri":)" + QuoteJson(mUri) + R"(},"position":{"line":)" + std::to_string(mCompletionPosition.line) + R"(,"character":)" + std::to_string(mCompletionPosition.index) + "}}}");
        State.suggestionsPromise = true;
        return;
    }

    constexpr std::string_view Bindings[]{"Actor", "Tasks", "World", "Input", "Key", "Vector", "Rotator", "Quat", "Transform", "BeginPlay", "Tick", "EndPlay", "OnOverlap", "OnEndOverlap", "OnHit"};
    const auto Add{[&State](std::string_view Text) {
        if (Text.starts_with(State.searchTerm) && Text != State.searchTerm) {
            State.suggestions.emplace_back(Text);
        }
    }};
    if (State.language != nullptr) {
        std::ranges::for_each(State.language->keywords, Add);
        std::ranges::for_each(State.language->identifiers, Add);
    }
    std::ranges::for_each(Bindings, Add);
    std::ranges::sort(State.suggestions);
    State.suggestions.erase(std::unique(State.suggestions.begin(), State.suggestions.end()), State.suggestions.end());
}

void FLuaIntellisence::CancelCompletion() {
    const auto Request{std::exchange(mCompletionRequest, 0)};
    if (Request != 0) {
        Notify("$/cancelRequest", R"({"id":)" + std::to_string(Request) + '}');
    }
}

void FLuaIntellisence::HandleMessage(const rapidjson::Value& Message) {
    const auto Method{String(Get(Message, "method"))};
    const auto& Id{Get(Message, "id")};
    if (!Method.empty() && !Id.IsNull()) {
        HandleRequest(Message);
    } else if (Method == "textDocument/publishDiagnostics") {
        ApplyDiagnostics(Get(Message, "params"));
    } else if (Id.IsUint64() && Id.GetUint64() == mInitializeRequest) {
        mInitializeRequest = 0;
        mReady = Get(Message, "result").IsObject();
        if (mReady) {
            Notify("initialized", "{}");
        }
    } else if (Id.IsUint64() && Id.GetUint64() == mCompletionRequest) {
        mCompletionRequest = 0;
        if (mCompletionVersion == mVersion && mCompletionPosition == mEditor.GetCurrentCursorPosition()) {
            ApplyCompletions(Get(Message, "result"));
        }
    }
}

void FLuaIntellisence::HandleRequest(const rapidjson::Value& Message) {
    rapidjson::StringBuffer Buffer{};
    rapidjson::Writer<rapidjson::StringBuffer> Writer{Buffer};
    Get(Message, "id").Accept(Writer);
    const auto Method{String(Get(Message, "method"))};
    std::string Reply{R"({"jsonrpc":"2.0","id":)" + std::string{Buffer.GetString(), Buffer.GetSize()}};
    if (Method == "workspace/configuration") {
        const auto& Items{Get(Get(Message, "params"), "items")};
        Reply += R"(,"result":[)";
        if (Items.IsArray()) {
            for (rapidjson::SizeType Index{}; Index < Items.Size(); ++Index) {
                if (Index != 0) {
                    Reply += ',';
                }
                const auto Section{String(Get(Items[Index], "section"))};
                Reply += Section == "Lua" ? R"({"runtime":{"version":"LuaJIT","enableLuaJITExtensions":true},"workspace":{"checkThirdParty":false,"library":[)" + QuoteJson(mLibraryPath) + R"(]},"diagnostics":{"workspaceEvent":"None","severity":{"undefined-global":"Error","undefined-field":"Error"}}})" : "null";
            }
        }
        Reply += "]}";
    } else if (Method == "window/workDoneProgress/create" || Method == "client/registerCapability" || Method == "client/unregisterCapability" || Method == "window/showMessageRequest") {
        Reply += R"(,"result":null})";
    } else {
        Reply += R"(,"error":{"code":-32601,"message":"Method not found"}})";
    }
    mServer.Send(std::move(Reply));
}

void FLuaIntellisence::ApplyCompletions(const rapidjson::Value& Result) {
    const auto& Items{Result.IsArray() ? Result : Get(Result, "items")};
    std::vector<std::string> Suggestions{};
    std::vector<std::string> Labels{};
    if (Items.IsArray()) {
        for (const auto& Item : Items.GetArray()) {
            const auto& Format{Get(Item, "insertTextFormat")};
            const auto& Edits{Get(Item, "additionalTextEdits")};
            if ((Format.IsInt() && Format.GetInt() == 2) || (Edits.IsArray() && !Edits.Empty())) {
                continue;
            }
            const auto Label{String(Get(Item, "label"))};
            auto Insert{String(Get(Item, "insertText"))};
            if (Insert.empty()) {
                Insert = String(Get(Get(Item, "textEdit"), "newText"));
            }
            if (Insert.empty()) {
                Insert = Label;
            }
            if (!Insert.empty() && std::ranges::find(Suggestions, Insert) == Suggestions.end()) {
                Suggestions.emplace_back(Insert);
                Labels.emplace_back(Label.empty() ? Insert : Label);
            }
        }
    }
    mEditor.SetAutoCompleteSuggestions(Suggestions, Labels);
}

void FLuaIntellisence::ApplyDiagnostics(const rapidjson::Value& Parameters) {
    const auto& Version{Get(Parameters, "version")};
    const auto& Diagnostics{Get(Parameters, "diagnostics")};
    const auto& Uri{Get(Parameters, "uri")};
    if (!Uri.IsString() || _stricmp(Uri.GetString(), mUri.c_str()) != 0 || !Version.IsUint64() || Version.GetUint64() != mVersion || !Diagnostics.IsArray()) {
        return;
    }

    mEditor.ClearMarkers();
    mEditor.ClearSquiggles();
    mCheckedVersion = mVersion;
    for (const auto& Diagnostic : Diagnostics.GetArray()) {
        const auto& Range{Get(Diagnostic, "range")};
        TextEditor::DocPos Start{};
        TextEditor::DocPos End{};
        const auto Message{String(Get(Diagnostic, "message"))};
        const auto& Severity{Get(Diagnostic, "severity")};
        if ((Severity.IsInt() && Severity.GetInt() > 2) || Message.empty() || !ReadPosition(Get(Range, "start"), Start) || !ReadPosition(Get(Range, "end"), End) || Start.line >= mEditor.GetLineCount()) {
            continue;
        }

        const ImU32 Color{Severity.IsInt() && Severity.GetInt() == 2 ? IM_COL32(230, 174, 67, 255) : IM_COL32(242, 90, 90, 255)};
        if (Start < End && Start.index < mEditor.GetLineCodePoints(Start.line).size()) {
            mEditor.AddSquiggle(Start, End, ServerDiagnostic, Color, Message);
        } else {
            mEditor.AddMarker(Start.line, Color, 0, Message, Message);
        }
    }
}

void FLuaIntellisence::CheckSyntax() {
    mCheckedVersion = mVersion;
    mEditor.ClearMarkers();
    mEditor.ClearSquiggles(SyntaxDiagnostic);
    if (mSyntaxState == nullptr) {
        mSyntaxState = luaL_newstate();
    }
    if (mSyntaxState == nullptr) {
        return;
    }

    const int Result{luaL_loadbufferx(mSyntaxState, mSource.data(), mSource.size(), "@script", "t")};
    if (Result == LUA_ERRSYNTAX) {
        const std::string_view Error{lua_tostring(mSyntaxState, -1)};
        const auto First{Error.find(':')};
        const auto Last{First == std::string_view::npos ? First : Error.find(':', First + 1)};
        size_t Line{};
        if (Last != std::string_view::npos && std::from_chars(Error.data() + First + 1, Error.data() + Last, Line).ec == std::errc{} && Line != 0) {
            const auto Index{std::min(Line, mEditor.GetLineCount()) - 1};
            const auto Length{mEditor.GetLineCodePoints(Index).size()};
            constexpr ImU32 Color{IM_COL32(242, 90, 90, 255)};
            const auto Message{Error.substr(Last + 1)};
            mEditor.AddMarker(Index, Color, 0, Message, Message);
            if (Length != 0) {
                mEditor.AddSquiggle({Index, 0}, {Index, Length}, SyntaxDiagnostic, Color, Message);
            }
        }
    }
    lua_pop(mSyntaxState, 1);
}

void FLuaIntellisence::SyncDocument() {
    if (mUri.empty() || mSentVersion == mVersion) {
        return;
    }

    const std::string Document{R"({"textDocument":{"uri":)" + QuoteJson(mUri) + R"(,"version":)" + std::to_string(mVersion)};
    if (mSentVersion == 0) {
        Notify("textDocument/didOpen", Document + R"(,"languageId":"lua","text":)" + QuoteJson(mSource) + "}}");
    } else {
        Notify("textDocument/didChange", Document + R"(},"contentChanges":[{"text":)" + QuoteJson(mSource) + "}]}");
    }
    mSentVersion = mVersion;
}

void FLuaIntellisence::Notify(std::string_view Method, std::string Parameters) {
    mServer.Send(R"({"jsonrpc":"2.0","method":)" + QuoteJson(Method) + R"(,"params":)" + Parameters + '}');
}
