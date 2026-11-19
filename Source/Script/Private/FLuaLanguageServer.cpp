#include "FLuaLanguageServer.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

using namespace Script;

namespace {
    struct FHandleCloser {
        void operator()(HANDLE Handle) const;
    };

    void FHandleCloser::operator()(HANDLE Handle) const {
        CloseHandle(Handle);
    }

    using FHandle = std::unique_ptr<void, FHandleCloser>;

    bool MakePipe(FHandle& Read, FHandle& Write) {
        SECURITY_ATTRIBUTES Attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE ReadHandle{};
        HANDLE WriteHandle{};
        if (!CreatePipe(&ReadHandle, &WriteHandle, &Attributes, 0)) {
            return false;
        }

        Read.reset(ReadHandle);
        Write.reset(WriteHandle);
        return true;
    }

    bool WritePacket(HANDLE Output, std::string_view Packet) {
        while (!Packet.empty()) {
            DWORD Count{};
            if (!WriteFile(Output, Packet.data(), static_cast<DWORD>(std::min<size_t>(Packet.size(), 65536)), &Count, nullptr) || Count == 0) {
                return false;
            }

            Packet.remove_prefix(Count);
        }

        return true;
    }
}

FLuaLanguageServer::~FLuaLanguageServer() {
    Stop();
}

bool FLuaLanguageServer::Start(const std::filesystem::path& Directory, const std::filesystem::path& Cache) {
    Stop();

    const auto Executable{Directory / L"bin/lua-language-server.exe"};
    std::error_code Error{};
    if (!std::filesystem::is_regular_file(Executable, Error)) {
        return false;
    }

    std::filesystem::create_directories(Cache, Error);
    if (Error) {
        return false;
    }

    FHandle ChildInput{};
    FHandle ParentInput{};
    FHandle ParentOutput{};
    FHandle ChildOutput{};
    if (!MakePipe(ChildInput, ParentInput) || !MakePipe(ParentOutput, ChildOutput) || !SetHandleInformation(ParentInput.get(), HANDLE_FLAG_INHERIT, 0) || !SetHandleInformation(ParentOutput.get(), HANDLE_FLAG_INHERIT, 0)) {
        return false;
    }

    SECURITY_ATTRIBUTES Attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const HANDLE ErrorHandle{CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &Attributes, OPEN_EXISTING, 0, nullptr)};
    if (ErrorHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    const FHandle ChildError{ErrorHandle};

    SIZE_T AttributeSize{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &AttributeSize);
    const auto AttributeStorage{std::make_unique<std::byte[]>(AttributeSize)};
    auto* AttributeList{reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(AttributeStorage.get())};
    if (!InitializeProcThreadAttributeList(AttributeList, 1, 0, &AttributeSize)) {
        return false;
    }

    std::array<HANDLE, 3> Handles{ChildInput.get(), ChildOutput.get(), ChildError.get()};
    STARTUPINFOEXW Startup{};
    Startup.StartupInfo.cb = sizeof(Startup);
    Startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    Startup.StartupInfo.hStdInput = ChildInput.get();
    Startup.StartupInfo.hStdOutput = ChildOutput.get();
    Startup.StartupInfo.hStdError = ChildError.get();
    Startup.lpAttributeList = AttributeList;
    PROCESS_INFORMATION Process{};
    std::wstring Command{L"\"" + Executable.wstring() + L"\" \"--logpath=" + (Cache / L"Log").wstring() + L"\" \"--metapath=" + (Cache / L"Meta").wstring() + L"\""};
    const bool Started{UpdateProcThreadAttribute(AttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, Handles.data(), sizeof(Handles), nullptr, nullptr) && CreateProcessW(Executable.c_str(), Command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, Directory.c_str(), &Startup.StartupInfo, &Process)};
    DeleteProcThreadAttributeList(AttributeList);
    if (!Started) {
        return false;
    }

    CloseHandle(Process.hThread);
    mProcess = Process.hProcess;
    mInput = ParentInput.release();
    mOutput = ParentOutput.release();
    mRunning = true;
    try {
        mReader = std::jthread{[this]() {
            Read();
        }};
        mWriter = std::jthread{[this](std::stop_token Stop) {
            Write(Stop);
        }};
    } catch (const std::system_error&) {
        Stop();
        return false;
    }
    return true;
}

void FLuaLanguageServer::Stop() {
    if (mProcess == nullptr) {
        return;
    }

    mRunning = false;
    mWriter.request_stop();
    mQueued.notify_all();
    TerminateProcess(mProcess, 0);
    if (mReader.joinable()) {
        mReader.join();
    }
    if (mWriter.joinable()) {
        mWriter.join();
    }

    CloseHandle(std::exchange(mInput, nullptr));
    CloseHandle(std::exchange(mOutput, nullptr));
    CloseHandle(std::exchange(mProcess, nullptr));
    mIncoming.clear();
    mOutgoing.clear();
}

bool FLuaLanguageServer::IsRunning() const {
    return mRunning;
}

void FLuaLanguageServer::Send(std::string Message) {
    if (mRunning) {
        const std::lock_guard Lock{mMutex};
        mOutgoing.push_back(std::move(Message));
        mQueued.notify_one();
    }
}

std::vector<std::string> FLuaLanguageServer::TakeMessages() {
    const std::lock_guard Lock{mMutex};
    return std::exchange(mIncoming, {});
}

void FLuaLanguageServer::Read() {
    constexpr std::string_view ContentLength{"Content-Length: "};
    std::array<char, 8192> Bytes{};
    std::string Buffer{};
    DWORD Count{};
    while (mRunning && ReadFile(mOutput, Bytes.data(), static_cast<DWORD>(Bytes.size()), &Count, nullptr) && Count != 0) {
        Buffer.append(Bytes.data(), Count);
        for (;;) {
            const auto HeaderEnd{Buffer.find("\r\n\r\n")};
            if (HeaderEnd == std::string::npos && Buffer.size() <= Bytes.size()) {
                break;
            }

            const auto LengthStart{Buffer.find(ContentLength)};
            size_t Length{};
            if (HeaderEnd == std::string::npos || LengthStart == std::string::npos || LengthStart + ContentLength.size() >= HeaderEnd || std::from_chars(Buffer.data() + LengthStart + ContentLength.size(), Buffer.data() + HeaderEnd, Length).ec != std::errc{} || Length > 32 * 1024 * 1024) {
                mRunning = false;
                mQueued.notify_all();
                return;
            }

            const size_t Body{HeaderEnd + 4};
            if (Buffer.size() - Body < Length) {
                break;
            }

            {
                const std::lock_guard Lock{mMutex};
                mIncoming.emplace_back(Buffer.substr(Body, Length));
            }
            Buffer.erase(0, Body + Length);
        }
    }

    mRunning = false;
    mQueued.notify_all();
}

void FLuaLanguageServer::Write(std::stop_token Stop) {
    while (!Stop.stop_requested() && mRunning) {
        std::unique_lock Lock{mMutex};
        mQueued.wait(Lock, Stop, [this]() {
            return !mOutgoing.empty() || !mRunning;
        });
        auto Messages{std::exchange(mOutgoing, {})};
        Lock.unlock();

        for (const auto& Message : Messages) {
            if (Stop.stop_requested() || !WritePacket(mInput, "Content-Length: " + std::to_string(Message.size()) + "\r\n\r\n") || !WritePacket(mInput, Message)) {
                mRunning = false;
                return;
            }
        }
    }
}
