#pragma once

#include "Core/Log/LogCategory.h"

namespace Core {
    struct FLogRecord {
        Core::FString mCategory{};
        Core::ELogVerbosity::Type mVerbosity{};
        Core::FString mMessage{};

        Core::FString ToString() const;
    };

    class FOutputDevice {
    public:
        virtual ~FOutputDevice();

    public:
        virtual void Serialize(const Core::FLogRecord& Record) = 0;
    };
}
