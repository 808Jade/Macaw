#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <Windows.h>

namespace Script {
    class FLuaLanguageServer {
    public:
        FLuaLanguageServer() = default;
        ~FLuaLanguageServer();

    public:
        bool Start(const std::filesystem::path& Directory, const std::filesystem::path& Cache);
        void Stop();
        bool IsRunning() const;

        void Send(std::string Message);
        std::vector<std::string> TakeMessages();

    private:
        void Read();
        void Write(std::stop_token Stop);

    private:
        HANDLE mProcess{};
        HANDLE mInput{};
        HANDLE mOutput{};
        std::atomic_bool mRunning{};
        std::mutex mMutex{};
        std::condition_variable_any mQueued{};
        std::vector<std::string> mIncoming{};
        std::vector<std::string> mOutgoing{};
        std::jthread mReader{};
        std::jthread mWriter{};
    };
}
