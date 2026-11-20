#pragma once

#include "Core/Types.h"

#include <atomic>
#include <string_view>

namespace Core {
    namespace ELogVerbosity {
        enum Type : Uint8 {
            NoLogging,
            Fatal,
            Error,
            Warning,
            Display,
            Log,
            Verbose,
            VeryVerbose,
            All = VeryVerbose
        };
    }

    class FLogCategoryBase {
    public:
        FLogCategoryBase(std::string_view Name, Core::ELogVerbosity::Type DefaultVerbosity, Core::ELogVerbosity::Type MaximumVerbosity);

    public:
        std::string_view GetName() const;
        Core::ELogVerbosity::Type GetVerbosity() const;
        void SetVerbosity(Core::ELogVerbosity::Type Verbosity);
        bool IsSuppressed(Core::ELogVerbosity::Type Verbosity) const;

    private:
        Core::FString mName{};
        Core::ELogVerbosity::Type mMaximumVerbosity{};
        std::atomic<Core::ELogVerbosity::Type> mVerbosity{};
    };

    template <Core::ELogVerbosity::Type DefaultVerbosity, Core::ELogVerbosity::Type MaximumVerbosity>
    class FLogCategory : public Core::FLogCategoryBase {
    public:
        explicit FLogCategory(std::string_view Name);

    public:
        static constexpr Core::ELogVerbosity::Type CompileTimeVerbosity{MaximumVerbosity};
    };

    template <Core::ELogVerbosity::Type DefaultVerbosity, Core::ELogVerbosity::Type MaximumVerbosity>
    FLogCategory<DefaultVerbosity, MaximumVerbosity>::FLogCategory(std::string_view Name)
        : Core::FLogCategoryBase{Name, DefaultVerbosity, MaximumVerbosity} {
    }
}

#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) extern ::Core::FLogCategory<::Core::ELogVerbosity::DefaultVerbosity, ::Core::ELogVerbosity::CompileTimeVerbosity> CategoryName
#define DEFINE_LOG_CATEGORY(CategoryName) decltype(CategoryName) CategoryName{#CategoryName}
#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity) static ::Core::FLogCategory<::Core::ELogVerbosity::DefaultVerbosity, ::Core::ELogVerbosity::CompileTimeVerbosity> CategoryName{#CategoryName}
