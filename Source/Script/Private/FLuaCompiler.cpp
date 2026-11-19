#include "Script/FLuaCompiler.h"

#include "Core/FilePath.h"

#include <lua.hpp>

using namespace Script;

FLuaCompiler::~FLuaCompiler() {
    if (mState != nullptr) {
        lua_close(mState);
    }
}

bool FLuaCompiler::CheckSyntax(const std::filesystem::path& Path, std::string_view Source, std::string& Error) {
    if (mState == nullptr) {
        mState = luaL_newstate();
    }
    if (mState == nullptr) {
        Error = "Could not initialize the LuaJIT compiler.";

        return false;
    }

    const std::string ChunkName{"@" + Core::FilePath::ToUtf8(Path.filename())};
    const bool Succeeded{luaL_loadbufferx(mState, Source.empty() ? "" : Source.data(), Source.size(), ChunkName.c_str(), "t") == 0};
    if (Succeeded) {
        Error.clear();
    } else {
        const char* Message{lua_tostring(mState, -1)};
        Error = Message != nullptr ? Message : "The LuaJIT compiler failed.";
    }
    lua_pop(mState, 1);

    return Succeeded;
}
