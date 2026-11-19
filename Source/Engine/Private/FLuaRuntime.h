#pragma once

#include "Core/FPathTracker.h"
#include "Core/Input.h"
#include "CoreUObject/TObjectRef.h"
#include "Engine/AActor.h"
#include "Engine/Collision/FHitResult.h"

#include <lua.hpp>
#include <sol/sol.hpp>

#include <utility>

namespace Engine {

    class ULuaScriptComponent;
    class USkeletalMeshComponent;
    class UWorld;

    class FLuaScriptInstance {
    private:
        struct FEvent {
            Engine::AActor::EComponentCollision mKind{};
            CoreUObject::TObjectRef<Engine::AActor> mOther{};
            Engine::FHitResult mHit{};
        };

        struct FExecution {
            sol::environment mEnvironment{};
            sol::table mTasks{};
            sol::protected_function mBeginPlay{};
            sol::protected_function mTick{};
            sol::protected_function mEndPlay{};
            sol::protected_function mOverlap{};
            sol::protected_function mEndOverlap{};
            sol::protected_function mHit{};
            sol::protected_function mAdvanceTasks{};
        };

    private:
        friend class FLuaRuntime;

        void DisconnectCollision();

    private:
        std::filesystem::path mPath{};
        CoreUObject::TObjectRef<Engine::ULuaScriptComponent> mComponent{};
        CoreUObject::TObjectRef<Engine::AActor> mOwner{};
        CoreUObject::FDelegateHandle mCollisionHandle{};
        FExecution mExecution{};
        std::vector<FEvent> mEvents{};
        bool mRunning{};
    };

    class FLuaRuntime {
    private:
        struct FActorRef {
            CoreUObject::TObjectRef<Engine::AActor> mActor{};

            Engine::AActor& Resolve() const;
        };

        struct FSkeletalMeshRef {
            CoreUObject::TObjectRef<Engine::USkeletalMeshComponent> mComponent{};

            Engine::USkeletalMeshComponent& Resolve() const;
        };

        struct FSource {
            sol::protected_function mChunk{};
            std::size_t mUsers{};
        };

    public:
        explicit FLuaRuntime(Engine::UWorld& World);
        ~FLuaRuntime();

    public:
        Engine::FLuaScriptInstance* Start(Engine::ULuaScriptComponent& Component, const std::filesystem::path& Path);
        static void Tick(Engine::FLuaScriptInstance& Instance, float DeltaTime);
        void Stop(Engine::ULuaScriptComponent& Component, Engine::FLuaScriptInstance& Instance);
        void ReloadChangedScripts();

        void SetFrameInput(const Core::FInput& Input);
        void ClearFrameInput();

    private:
        void RegisterBindings();
        sol::protected_function LoadChunk(const std::filesystem::path& Path);
        bool CreateExecution(const Engine::FLuaScriptInstance& Instance, const sol::protected_function& Chunk, Engine::FLuaScriptInstance::FExecution& Execution);
        void Reload(const std::filesystem::path& Path, FSource& Source);

        template <typename... Args>
        static bool Invoke(const Engine::FLuaScriptInstance& Instance, const sol::protected_function& Function, const char* Event, Args&&... Arguments);

        static bool FinishCall(const Engine::FLuaScriptInstance& Instance, const char* Event, const sol::protected_function_result& Result);
        static void Report(const std::filesystem::path& Path, const Engine::AActor* Owner, const char* Event, std::string_view Message);

    private:
        Engine::UWorld& mWorld;
        Core::FInput mFrameInput{};
        sol::state mLua{};
        sol::protected_function mMakeTasks{};
        sol::protected_function mCaptureCallbacks{};
        std::map<std::filesystem::path, FSource> mSources{};
        std::map<Engine::ULuaScriptComponent*, Engine::FLuaScriptInstance> mInstances{};
        Core::FPathTracker mPathTracker{};
    };

    template <typename... Args>
    bool Engine::FLuaRuntime::Invoke(const Engine::FLuaScriptInstance& Instance, const sol::protected_function& Function, const char* Event, Args&&... Arguments) {
        return !Function.valid() || FinishCall(Instance, Event, Function(std::forward<Args>(Arguments)...));
    }

}
