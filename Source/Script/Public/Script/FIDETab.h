#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct ImFont;
class TextEditor;

namespace Script {
    class FIDETab {
    public:
        FIDETab();
        ~FIDETab();

    public:
        bool Open(const std::filesystem::path& Path);
        bool Save();
        void DiscardChanges();

        const std::filesystem::path& GetPath() const;
        bool IsDirty() const;

        void Draw();

    private:
        std::string GetSourceText() const;

    private:
        std::unique_ptr<TextEditor> mEditor{};
        ImFont* mFont{};
        std::filesystem::path mPath{};
        std::string mSavedSource{};
        std::string mSavedText{};
        std::string_view mLineEnding{"\n"};
        std::string mSaveError{};
        bool mDirty{};
    };
}
