#include "FLuaRuntime.h"

#include "Core/Base/ErrorHandler.h"
#include "Core/FilePath.h"
#include "Engine/AActor.h"
#include "Engine/Component/ULuaScriptComponent.h"
#include "LuaBootstrap.generated.h"

#include <fstream>
#include <iterator>
#include <string_view>

#pragma comment(lib, "lua51.lib")

using namespace Engine;

FLuaRuntime::FLuaRuntime(Engine::UWorld& World)
    : mWorld{World} {
    mLua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::jit);
    RegisterBindings();

    const std::string_view Source{reinterpret_cast<const char*>(Engine::LuaBootstrapSource), sizeof(Engine::LuaBootstrapSource)};
    const auto Bootstrap{mLua.script(Source, "@Source/Engine/Private/Lua/Bootstrap.lua")};
    const sol::table Runtime{Bootstrap.get<sol::table>()};
    mMakeTasks = Runtime["MakeTasks"];
    mCaptureCallbacks = Runtime["CaptureCallbacks"];
}

FLuaRuntime::~FLuaRuntime() {
    for (auto& Entry : mInstances) {
        auto& Instance{Entry.second};
        if (auto* Component{Instance.mComponent.Get()}; Component != nullptr && Component->mRuntimeInstance == &Instance) {
            Component->mRuntimeInstance = nullptr;
        }
        Instance.DisconnectCollision();
    }
}

void FLuaScriptInstance::DisconnectCollision() {
    if (auto* Owner{mOwner.Get()}; Owner != nullptr && mCollisionHandle != 0) {
        Owner->GetOnComponentCollision().Remove(mCollisionHandle);
    }
    mCollisionHandle = {};
}

Engine::FLuaScriptInstance* FLuaRuntime::Start(Engine::ULuaScriptComponent& Component, const std::filesystem::path& Path) {
    auto* Owner{Component.GetOwner()};
    if (Owner == nullptr) {
        return nullptr;
    }

    auto [Instance, Added]{mInstances.try_emplace(&Component)};
    auto& Value{Instance->second};
    if (!Added) {
        return &Value;
    }
    Value.mPath = Path;
    Value.mComponent = CoreUObject::MakeObjectRef(&Component);
    Value.mOwner = CoreUObject::MakeObjectRef(Owner);

    auto [Source, FirstUse]{mSources.try_emplace(Path)};
    if (FirstUse) {
        mPathTracker.Track(Path);
        Source->second.mChunk = LoadChunk(Path);
    }
    ++Source->second.mUsers;
    Value.mCollisionHandle = Owner->GetOnComponentCollision().Add([Target = &Value](Engine::AActor::EComponentCollision Kind, Engine::UPrimitiveComponent*, Engine::AActor* Other, Engine::UPrimitiveComponent*, const Engine::FHitResult* Hit) {
        if (Target->mRunning) {
            Target->mEvents.push_back({Kind, CoreUObject::MakeObjectRef(Other), Hit != nullptr ? *Hit : Engine::FHitResult{}});
        }
    });
    Engine::FLuaScriptInstance::FExecution Execution{};
    if (CreateExecution(Value, Source->second.mChunk, Execution)) {
        Value.mExecution = std::move(Execution);
        Value.mRunning = true;
        Value.mRunning = Invoke(Value, Value.mExecution.mBeginPlay, "BeginPlay");
    }
    return &Value;
}

void FLuaRuntime::Tick(Engine::FLuaScriptInstance& Instance, float DeltaTime) {
    if (!Instance.mRunning) {
        return;
    }

    auto& Execution{Instance.mExecution};
    auto Events{std::move(Instance.mEvents)};
    Instance.mEvents.clear();
    for (const auto& Event : Events) {
        switch (Event.mKind) {
        case Engine::AActor::EComponentCollision::BeginOverlap:
            Instance.mRunning = Invoke(Instance, Execution.mOverlap, "OnOverlap", FActorRef{Event.mOther});
            break;
        case Engine::AActor::EComponentCollision::EndOverlap:
            Instance.mRunning = Invoke(Instance, Execution.mEndOverlap, "OnEndOverlap", FActorRef{Event.mOther});
            break;
        case Engine::AActor::EComponentCollision::Hit:
            Instance.mRunning = Invoke(Instance, Execution.mHit, "OnHit", FActorRef{Event.mOther}, Engine::FHitResult{Event.mHit});
            break;
        }
        if (!Instance.mRunning) {
            return;
        }
    }

    Instance.mRunning = Invoke(Instance, Execution.mTick, "Tick", DeltaTime) && Invoke(Instance, Execution.mAdvanceTasks, "Coroutine", Execution.mTasks, DeltaTime);
}

void FLuaRuntime::Stop(Engine::ULuaScriptComponent& Component, Engine::FLuaScriptInstance& Instance) {
    Instance.DisconnectCollision();
    Invoke(Instance, Instance.mExecution.mEndPlay, "EndPlay");

    const auto Source{mSources.find(Instance.mPath)};
    if (--Source->second.mUsers == 0) {
        mPathTracker.Untrack(Source->first);
        mSources.erase(Source);
    }
    mInstances.erase(&Component);
}

void FLuaRuntime::ReloadChangedScripts() {
    for (const auto& Path : mPathTracker.TakeChanges()) {
        if (const auto Source{mSources.find(Path)}; Source != mSources.end()) {
            Reload(Source->first, Source->second);
        }
    }
}

void FLuaRuntime::SetFrameInput(const Core::FInput& Input) {
    mFrameInput = Input;
}

void FLuaRuntime::ClearFrameInput() {
    mFrameInput = {};
}

sol::protected_function FLuaRuntime::LoadChunk(const std::filesystem::path& Path) {
    std::ifstream File{Path, std::ios::binary};
    if (!File) {
        Report(Path, nullptr, "Load", "Could not open the script");
        return {};
    }

    const std::string Buffer{std::istreambuf_iterator<char>{File}, std::istreambuf_iterator<char>{}};
    std::string_view Code{Buffer};
    if (Code.starts_with("\xEF\xBB\xBF")) {
        Code.remove_prefix(3);
    }

    const sol::load_result Loaded{mLua.load(Code, "@" + Core::FilePath::ToGenericUtf8(Path), sol::load_mode::text)};
    if (!Loaded.valid()) {
        const auto Error{Loaded.get<sol::error>()};
        Report(Path, nullptr, "Compile", Error.what());
        return {};
    }

    return Loaded.get<sol::protected_function>();
}

bool FLuaRuntime::CreateExecution(const Engine::FLuaScriptInstance& Instance, const sol::protected_function& Chunk, Engine::FLuaScriptInstance::FExecution& Execution) {
    if (!Chunk.valid()) {
        return false;
    }

    const auto Tasks{mMakeTasks()};
    if (!FinishCall(Instance, "Create coroutine context", Tasks)) {
        return false;
    }
    Execution.mTasks = Tasks.get<sol::table>();
    Execution.mAdvanceTasks = Tasks.get<sol::protected_function>(1);

    Execution.mEnvironment = sol::environment{mLua, sol::create, mLua.globals()};
    Execution.mEnvironment["_G"] = Execution.mEnvironment;
    Execution.mEnvironment["Actor"] = FActorRef{Instance.mOwner};
    Execution.mEnvironment["Tasks"] = Execution.mTasks;

    const auto PreviousEnvironment{sol::get_environment(Chunk)};
    sol::set_environment(Execution.mEnvironment, Chunk);
    const auto Result{Chunk()};
    sol::set_environment(PreviousEnvironment, Chunk);
    if (!FinishCall(Instance, "Initialize", Result)) {
        return false;
    }
    if (Result.return_count() != 0 && Result.get_type() == sol::type::function) {
        Report(Instance.mPath, Instance.mOwner.Get(), "Initialize", "Define lifecycle callbacks directly in the script environment.");
        return false;
    }
    const auto Captured{mCaptureCallbacks(Execution.mEnvironment)};
    if (!FinishCall(Instance, "Capture callbacks", Captured)) {
        return false;
    }

    const auto Callbacks{Captured.get<sol::table>()};
    const auto GetCallback{[&Callbacks](const char* Name) {
        return Callbacks.get<sol::optional<sol::protected_function>>(Name).value_or(sol::protected_function{});
    }};
    Execution.mBeginPlay = GetCallback("BeginPlay");
    Execution.mTick = GetCallback("Tick");
    Execution.mEndPlay = GetCallback("EndPlay");
    Execution.mOverlap = GetCallback("OnOverlap");
    Execution.mEndOverlap = GetCallback("OnEndOverlap");
    Execution.mHit = GetCallback("OnHit");
    return true;
}

void FLuaRuntime::Reload(const std::filesystem::path& Path, FSource& Source) {
    auto Chunk{LoadChunk(Path)};
    if (!Chunk.valid()) {
        return;
    }

    std::vector<std::pair<Engine::FLuaScriptInstance*, Engine::FLuaScriptInstance::FExecution>> Replacements{};
    for (auto& Entry : mInstances) {
        auto& Instance{Entry.second};
        if (Instance.mPath != Path) {
            continue;
        }

        Engine::FLuaScriptInstance::FExecution Replacement{};
        if (!CreateExecution(Instance, Chunk, Replacement)) {
            return;
        }
        Replacements.emplace_back(&Instance, std::move(Replacement));
    }

    Source.mChunk = std::move(Chunk);
    for (auto& [Target, Replacement] : Replacements) {
        auto& Instance{*Target};
        auto& Execution{Instance.mExecution};
        Invoke(Instance, Execution.mEndPlay, "EndPlay");
        Execution = std::move(Replacement);
        Instance.mRunning = true;
        Instance.mRunning = Invoke(Instance, Execution.mBeginPlay, "BeginPlay");
    }
}

bool FLuaRuntime::FinishCall(const Engine::FLuaScriptInstance& Instance, const char* Event, const sol::protected_function_result& Result) {
    if (!Result.valid()) {
        const auto Error{Result.get<sol::error>()};
        Report(Instance.mPath, Instance.mOwner.Get(), Event, Error.what());
        return false;
    }
    return true;
}

void FLuaRuntime::Report(const std::filesystem::path& Path, const Engine::AActor* Owner, const char* Event, std::string_view Message) {
    auto Context{Core::FilePath::ToGenericUtf8(Path) + " [" + Event + "] "};
    if (Owner != nullptr) {
        Context += Owner->GetName() + ": ";
    }
    Context += Message;
    Core::ErrorHandler::Report("Lua", Context, Core::ErrorHandler::EErrorLevel::Warning);
}
