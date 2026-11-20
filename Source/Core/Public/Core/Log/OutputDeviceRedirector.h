#pragma once

#include "Core/Log/OutputDevice.h"

#include <deque>
#include <mutex>

namespace Core {
    class FOutputDeviceRedirector : public Core::FOutputDevice {
    public:
        FOutputDeviceRedirector();
        ~FOutputDeviceRedirector() override;

    public:
        static Core::FOutputDeviceRedirector* Get();

        void AddOutputDevice(const std::shared_ptr<Core::FOutputDevice>& Device);
        void RemoveOutputDevice(const std::shared_ptr<Core::FOutputDevice>& Device);
        void Serialize(const Core::FLogRecord& Record) override;
        Core::TArray<Core::FLogRecord> ReadBacklog(Uint64& Sequence) const;

    private:
        mutable std::mutex mMutex{};
        Core::TArray<std::shared_ptr<Core::FOutputDevice>> mOutputDevices{};
        std::deque<Core::FLogRecord> mBacklog{};
        Uint64 mSequence{};
    };
}

#define GLog (::Core::FOutputDeviceRedirector::Get())
