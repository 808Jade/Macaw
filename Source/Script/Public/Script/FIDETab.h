#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct ImFont;
class TextEditor;

namespace Script {
    class FLuaIntellisence;

    class FIDETab {
    public:
        FIDETab();
        ~FIDETab();

    public:
        bool Open(const std::filesystem::path& Path, const std::filesystem::path& Workspace);
        bool Save();
        void Close();

        const std::filesystem::path& GetPath() const;
        bool IsDirty() const;

        void Draw();

    private:
        std::string GetSourceText() const;
        void DiscardChanges();

    private:
        std::unique_ptr<TextEditor> mEditor{};
        std::unique_ptr<Script::FLuaIntellisence> mIntellisence{};
        ImFont* mFont{};
        std::filesystem::path mPath{};
        std::string mSavedSource{};
        std::string mSavedText{};
        std::string_view mLineEnding{"\n"};
        std::string mSaveError{};
        bool mDirty{};
    };
}
