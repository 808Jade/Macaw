#include "Script/FIDETab.h"
#include "FLuaIntellisence.h"
#include "Core/FAtomicFileWriter.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <fstream>
#include <utility>

using namespace Script;

namespace {
    constexpr std::string_view Utf8Bom{"\xEF\xBB\xBF"};

    bool ReadSource(const std::filesystem::path& Path, std::string& Source) {
        std::ifstream File{Path, std::ios::binary | std::ios::ate};
        const std::streamoff Size{File.tellg()};

        if (Size < 0) {
            return false;
        }

        Source.resize(static_cast<std::size_t>(Size));
        return File.seekg(0) && File.read(Source.data(), Size);
    }

    std::string ConvertLineEndings(std::string_view Text, std::string_view LineEnding) {
        std::string Result{};
        Result.reserve(Text.size());

        while (!Text.empty()) {
            const auto End{Text.find_first_of("\r\n")};
            Result += Text.substr(0, End);

            if (End == std::string_view::npos) {
                break;
            }

            Result += LineEnding;
            Text.remove_prefix(End + (Text.substr(End).starts_with("\r\n") ? 2 : 1));
        }

        return Result;
    }
}

FIDETab::FIDETab()
	: mEditor{std::make_unique<TextEditor>()},
	  mIntellisence{std::make_unique<FLuaIntellisence>(*mEditor)},
	  mFont{ImGui::GetIO().Fonts->AddFontDefaultVector()} {
    constexpr char KoreanFont[]{"C:/Windows/Fonts/malgun.ttf"};
    std::error_code Error{};

    if (std::filesystem::is_regular_file(KoreanFont, Error)) {
        ImFontConfig Config{};
        Config.MergeMode = true;
        Config.DstFont = mFont;
        ImGui::GetIO().Fonts->AddFontFromFileTTF(KoreanFont, 0.0f, &Config);
    }

    mEditor->SetLanguage(TextEditor::LookupLanguage("Lua"));
    mEditor->SetTransactionCallback([this](const std::vector<TextEditor::Change>&) {
        auto Text{GetSourceText()};
        mDirty = Text != mSavedText;
        if (!mDirty) {
            mSaveError.clear();
        }
        if (!mEditor->IsReadOnlyEnabled()) {
            mIntellisence->Edit(std::move(Text));
        }
    });
}

FIDETab::~FIDETab() = default;

bool FIDETab::Open(const std::filesystem::path& Path, const std::filesystem::path& Workspace) {
    std::string Source{};
    if (!ReadSource(Path, Source)) {
        return false;
    }

    std::string_view Code{Source};
    if (Code.starts_with(Utf8Bom)) {
        Code.remove_prefix(Utf8Bom.size());
    }

    mLineEnding = "\n";
    if (const auto FirstEnding{Code.find_first_of("\r\n")}; FirstEnding != std::string_view::npos && Code[FirstEnding] == '\r') {
        mLineEnding = Code.substr(FirstEnding).starts_with("\r\n") ? "\r\n" : "\r";
    }

    const bool Binary{Code.find('\0') != std::string_view::npos};
    mSavedText = Binary ? std::string{} : ConvertLineEndings(Code, "\n");
    mSavedSource = std::move(Source);
    mPath = Path;
    mEditor->SetText(mSavedText);
    const bool ReadOnly{Binary || GetSourceText() != mSavedText};
    mEditor->SetReadOnlyEnabled(ReadOnly);
    if (ReadOnly) {
        mIntellisence->Close();
    } else {
        mIntellisence->Open(Path, Workspace, mSavedText);
    }
    mEditor->SetFocus();
    mSaveError.clear();
    mDirty = false;

    return true;
}

bool FIDETab::Save() {
    if (!mDirty) {
        return true;
    }

    if (mEditor->IsReadOnlyEnabled()) {
        return false;
    }

    std::string DiskSource{};
    if (!ReadSource(mPath, DiskSource)) {
        mSaveError = "Cannot read the current script file.";
        return false;
    }

    if (DiskSource != mSavedSource) {
        mSaveError = "The script changed on disk. Your edits are still open.";
        return false;
    }

    std::string Text{GetSourceText()};
    std::string Source{mSavedSource.starts_with(Utf8Bom) ? Utf8Bom : std::string_view{}};
    Source += ConvertLineEndings(Text, mLineEnding);

    Core::FAtomicFileWriter Writer{};
    if (!Writer.Open(mPath, mSaveError)) {
        return false;
    }

    Writer.GetStream().write(Source.data(), static_cast<std::streamsize>(Source.size()));
    if (!Writer.Commit(mSaveError)) {
        return false;
    }

    mSavedSource = std::move(Source);
    mSavedText = std::move(Text);
    mDirty = false;
    mIntellisence->Save();

    return true;
}

void FIDETab::Close() {
    mIntellisence->Close();
    DiscardChanges();
}

void FIDETab::DiscardChanges() {
    if (mDirty) {
        mEditor->SetText(mSavedText);
        mDirty = false;
    }

    mSaveError.clear();
}

const std::filesystem::path& FIDETab::GetPath() const {
    return mPath;
}

bool FIDETab::IsDirty() const {
    return mDirty;
}

void FIDETab::Draw() {
    mIntellisence->Update();
    const bool ReadOnly{mEditor->IsReadOnlyEnabled()};
    if (ReadOnly) {
        ImGui::TextDisabled("Read-only: this file contains text the editor cannot preserve.");
    } else if (!mSaveError.empty()) {
        ImGui::TextWrapped("Save failed: %s", mSaveError.c_str());
    }

    ImGui::PushFont(mFont, 0.0f);
    mEditor->Render("##LuaSource");
    ImGui::PopFont();

    if (ReadOnly && mDirty) {
        DiscardChanges();
    }
}

std::string FIDETab::GetSourceText() const {
    return mEditor->GetSectionText({}, {mEditor->GetLineCount(), 0});
}
