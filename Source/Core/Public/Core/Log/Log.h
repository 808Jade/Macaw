#pragma once

#include "Core/Log/LogCategory.h"

#include <format>
#include <source_location>
#include <type_traits>

namespace Core::Log {
    void Write(const Core::FLogCategoryBase& Category, Core::ELogVerbosity::Type Verbosity, const std::source_location& Location, Core::FString Message);
}

DECLARE_LOG_CATEGORY_EXTERN(LogTemp, Log, All);

#define MACAW_LOG(CategoryName, Verbosity, Format, ...) \
    do { \
        static_assert(::Core::ELogVerbosity::Verbosity >= ::Core::ELogVerbosity::Fatal && ::Core::ELogVerbosity::Verbosity <= ::Core::ELogVerbosity::VeryVerbose); \
        if constexpr (::Core::ELogVerbosity::Verbosity == ::Core::ELogVerbosity::Fatal || ::Core::ELogVerbosity::Verbosity <= ::std::remove_cvref_t<decltype(CategoryName)>::CompileTimeVerbosity) { \
            if (!(CategoryName).IsSuppressed(::Core::ELogVerbosity::Verbosity)) { \
                ::Core::Log::Write((CategoryName), ::Core::ELogVerbosity::Verbosity, ::std::source_location::current(), ::std::format((Format) __VA_OPT__(,) __VA_ARGS__)); \
            } \
        } \
    } while (false)

#define UE_LOG(CategoryName, Verbosity, Format, ...) MACAW_LOG(CategoryName, Verbosity, Format __VA_OPT__(,) __VA_ARGS__)
