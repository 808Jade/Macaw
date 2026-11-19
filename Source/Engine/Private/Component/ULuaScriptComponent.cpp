#include "Engine/Component/ULuaScriptComponent.h"

#include "Asset/FAssetManager.h"
#include "Core/Base/ErrorHandler.h"
#include "Core/FilePath.h"
#include "Engine/UWorld.h"
#include "FLuaRuntime.h"

#include <utility>

using namespace Engine;

ULuaScriptComponent::ULuaScriptComponent() = default;
ULuaScriptComponent::~ULuaScriptComponent() = default;

void ULuaScriptComponent::BeginPlay() {
    if (!mEnabled || mScriptPath.empty()) {
        return;
    }

    const auto Path{ResolveScriptPath()};
    if (Path.empty()) {
        Core::ErrorHandler::Report("Lua", "Invalid script path: " + mScriptPath, Core::ErrorHandler::EErrorLevel::Warning);
        return;
    }

    auto& Runtime{GetWorld()->mLuaRuntime};
    if (Runtime == nullptr) {
        Runtime = std::make_unique<Engine::FLuaRuntime>(*GetWorld());
    }

    mRuntimeInstance = Runtime->Start(*this, Path);
}

void ULuaScriptComponent::Tick(float DeltaTime) {
    if (mRuntimeInstance != nullptr && HasBegunPlay()) {
        Engine::FLuaRuntime::Tick(*mRuntimeInstance, DeltaTime);
    }
}

void ULuaScriptComponent::EndPlay() {
    auto* Instance{std::exchange(mRuntimeInstance, nullptr)};
    if (Instance != nullptr) {
        GetWorld()->mLuaRuntime->Stop(*this, *Instance);
    }
}

const Core::FString& ULuaScriptComponent::GetScriptPath() const {
    return mScriptPath;
}

bool ULuaScriptComponent::SetScriptPath(const Core::FString& Path) {
    if (HasBegunPlay()) {
        return false;
    }

    if (Path.empty()) {
        mScriptPath.clear();
        return true;
    }

    const auto Relative{Core::FilePath::FromUtf8(Path).lexically_normal()};
    if (Relative.has_root_path() || Relative.begin() == Relative.end() || *Relative.begin() == L".." || _wcsicmp(Relative.extension().c_str(), L".lua") != 0) {
        return false;
    }

    mScriptPath = Core::FilePath::ToGenericUtf8(Relative);
    return true;
}

std::filesystem::path ULuaScriptComponent::ResolveScriptPath() const {
    const auto* World{GetWorld()};
    if (mScriptPath.empty() || World == nullptr || World->GetAssets() == nullptr) {
        return {};
    }

    Core::FString Error{};
    return World->GetAssets()->ResolvePath(Core::FilePath::FromUtf8(mScriptPath), Error);
}

bool ULuaScriptComponent::IsEnabled() const {
    return mEnabled;
}

bool ULuaScriptComponent::SetEnabled(bool Enabled) {
    if (HasBegunPlay()) {
        return false;
    }

    mEnabled = Enabled;
    return true;
}
