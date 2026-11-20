#include "Core/Log/Log.h"

#include "Core/Base/ErrorHandler.h"
#include "Core/Log/OutputDeviceRedirector.h"

#include <utility>

DEFINE_LOG_CATEGORY(LogTemp);

namespace Core::Log {
    void Write(const Core::FLogCategoryBase& Category, Core::ELogVerbosity::Type Verbosity, const std::source_location& Location, Core::FString Message) {
        const Core::FLogRecord Record{Core::FString{Category.GetName()}, Verbosity, std::move(Message)};
        GLog->Serialize(Record);

        if (Verbosity == Core::ELogVerbosity::Fatal) {
            Core::ErrorHandler::Fatal(Record.mCategory, Record.mMessage, Location);
        }
    }
}
