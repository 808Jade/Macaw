#include "Core/Log/LogCategory.h"

#include <algorithm>

namespace Core {
    FLogCategoryBase::FLogCategoryBase(std::string_view Name, Core::ELogVerbosity::Type DefaultVerbosity, Core::ELogVerbosity::Type MaximumVerbosity)
        : mName{Name},
          mMaximumVerbosity{MaximumVerbosity},
          mVerbosity{std::min(DefaultVerbosity, MaximumVerbosity)} {
    }

    std::string_view FLogCategoryBase::GetName() const {
        return mName;
    }

    Core::ELogVerbosity::Type FLogCategoryBase::GetVerbosity() const {
        return mVerbosity.load(std::memory_order_relaxed);
    }

    void FLogCategoryBase::SetVerbosity(Core::ELogVerbosity::Type Verbosity) {
        mVerbosity.store(std::min(Verbosity, mMaximumVerbosity), std::memory_order_relaxed);
    }

    bool FLogCategoryBase::IsSuppressed(Core::ELogVerbosity::Type Verbosity) const {
        return Verbosity != Core::ELogVerbosity::Fatal && Verbosity > GetVerbosity();
    }
}
