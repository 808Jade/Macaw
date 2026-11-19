#pragma once

#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace Core {

    class FPathTracker {
    private:
        struct FStamp {
            std::filesystem::file_time_type mWriteTime{};
            std::uintmax_t mSize{};

            bool operator==(const FStamp& Other) const;
        };

        struct FWatch {
            std::optional<FStamp> mStamp{};
            std::size_t mId{};
            bool mChanged{};
        };

    public:
        FPathTracker();
        ~FPathTracker();

    public:
        void Track(const std::filesystem::path& Path);
        void Untrack(const std::filesystem::path& Path);
        std::vector<std::filesystem::path> TakeChanges();

    private:
        static std::optional<FStamp> ReadStamp(const std::filesystem::path& Path);
        void Poll(std::stop_token Stop);

    private:
        std::mutex mMutex{};
        std::condition_variable_any mWake{};
        std::map<std::filesystem::path, FWatch> mWatches{};
        std::size_t mNextId{};
        std::jthread mWorker{};
    };

}
