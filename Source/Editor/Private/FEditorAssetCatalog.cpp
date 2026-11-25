/**
 * @file FEditorAssetCatalog.cpp
 * @brief 실제 Content 경로를 사용하는 에셋 목록의 구현입니다.
 * @ingroup Editor
 */

#include "Core/Types.h"
#include "Editor/FEditorAssetCatalog.h"
#include "Core/FilePath.h"
#include "Asset/FAssetManager.h"

#include <algorithm>

namespace Editor {

    namespace {
        std::filesystem::path GetTextureAssetPath(const std::filesystem::path& Source) {
            const auto Parent{Core::FilePath::ToLower(Source.parent_path())};

            if (!Asset::FAssetManager::IsTextureSource(Source) || Parent == std::filesystem::path{L"editor/icon"} || Core::FilePath::ToLower(Source) == std::filesystem::path{L"fonts/default.png"}) {
                return {};
            }

            auto Target{Source};
            Target += L".masset";

            return Target;
        }
    }

    /**
     * @brief 에셋 항목을 만듭니다.
     */
    Editor::FEditorAssetEntry::FEditorAssetEntry(std::filesystem::path Path, std::optional<Asset::EAssetType> Type)
		: mPath{std::move(Path)},
		  mType{Type} {
    }

    /**
     * @brief 실제 참조 경로를 반환합니다.
     */
    const std::filesystem::path& Editor::FEditorAssetEntry::GetPath() const {
        return mPath;
    }

    /**
     * @brief 에셋 종류를 반환합니다.
     */
    std::optional<Asset::EAssetType> Editor::FEditorAssetEntry::GetType() const {
        return mType;
    }

    /**
     * @brief 프로젝트 관리자를 연결합니다.
     */
    Editor::FEditorAssetCatalog::FEditorAssetCatalog(Asset::FAssetManager* Assets)
		: mAssets{Assets} {
    }

    /**
     * @brief 비교용 경로 키를 만듭니다.
     */
    Core::FString Editor::FEditorAssetCatalog::MakeKey(const std::filesystem::path& Path) {
        return Core::FilePath::ToGenericUtf8(Core::FilePath::ToLower(Path.lexically_normal()));
    }

    /**
     * @brief Content의 폴더와 파일을 다시 읽습니다.
     */
    bool Editor::FEditorAssetCatalog::Refresh(Core::FString& Error) {
        Error.clear();
        mEntries.clear();
        mIndices.clear();
        mDirectories.clear();

        if (mAssets == nullptr) {
            Error = "The asset catalog requires an open project";

            return false;
        }

        const auto& Root{mAssets->GetContentRoot()};
        std::error_code Code{};
        std::filesystem::recursive_directory_iterator Iterator{Root, Code};
        const std::filesystem::recursive_directory_iterator End{};

        while (!Code && Iterator != End) {
            const auto Path{Iterator->path().lexically_relative(Root)};
            const bool Link{Iterator->is_symlink(Code)};
            const bool Directory{!Code && Iterator->is_directory(Code)};
            Core::FString PathError{};

            if (Link || mAssets->ResolvePath(Iterator->path(), PathError).empty()) {
                Iterator.disable_recursion_pending();
            } else if (Directory) {
                mDirectories.push_back(Path);
            } else if (!Code && Iterator->is_regular_file(Code)) {
                const std::wstring Extension{Core::FilePath::ToLowerExtension(Path)};

                std::optional<Asset::EAssetType> Type{};

                if (Extension == L".masset") {
                    Asset::EAssetType Value{};

                    if (Asset::FAssetFile::GetType(Iterator->path(), Value, PathError)) {
                        Type = Value;
                    } else if (Error.empty()) {
                        Error = Core::FilePath::ToUtf8(Path) + ": " + PathError;
                    }
                } else if (Asset::FAssetManager::IsTextureSource(Path)) {
                    Type = Asset::EAssetType::Texture;
                }

                mEntries.emplace_back(Path, Type);
            }

            if (!Code) {
                Iterator.increment(Code);
            }
        }

        std::sort(mEntries.begin(), mEntries.end(), [](const Editor::FEditorAssetEntry& First, const Editor::FEditorAssetEntry& Second) {
            return First.GetPath() < Second.GetPath();
        });

        mIndices.clear();

        for (Index_t Index{}; Index < mEntries.size(); ++Index) {
            mIndices.emplace(MakeKey(mEntries[Index].GetPath()), Index);
        }

        std::sort(mDirectories.begin(), mDirectories.end());
        mDirectories.erase(std::unique(mDirectories.begin(), mDirectories.end()), mDirectories.end());

        if (Code) {
            Error = "Cannot read Content: " + Code.message();
        }

        return !Code;
    }

    bool FEditorAssetCatalog::ImportLooseTextures(Core::FString& Error) {
        if (mAssets == nullptr) {
            return false;
        }

        bool Imported{};
        for (const auto& Entry : mEntries) {
            if (Entry.GetType() != Asset::EAssetType::Texture) {
                continue;
            }

            const auto Target{GetTextureAssetPath(Entry.GetPath())};
            if (Target.empty() || Find(Target) != nullptr) {
                continue;
            }

            const auto Source{mAssets->GetContentRoot() / Entry.GetPath()};
            Core::FString ImportError{};
            if (mAssets->ImportTexture(Source, Target, ImportError)) {
                Imported = true;
            } else if (Error.empty()) {
                Error = Core::FilePath::ToUtf8(Entry.GetPath()) + ": " + ImportError;
            }
        }

        return Imported;
    }

    bool FEditorAssetCatalog::IsImportedTextureSource(const std::filesystem::path& Path) const {
        const auto Target{GetTextureAssetPath(Path)};
        const auto* Entry{Target.empty() ? nullptr : Find(Target)};

        return Entry != nullptr && Entry->GetType() == Asset::EAssetType::Texture;
    }

    std::filesystem::path FEditorAssetCatalog::FindTextureSource(const std::filesystem::path& AssetPath) const {
        if (Core::FilePath::ToLowerExtension(AssetPath) != L".masset") {
            return {};
        }

        auto Source{AssetPath};
        Source.replace_extension();

        return GetTextureAssetPath(Source).empty() || Find(Source) == nullptr || !IsImportedTextureSource(Source) ? std::filesystem::path{} : Source;
    }

    /**
     * @brief 현재 항목들을 반환합니다.
     */
    const Core::TArray<Editor::FEditorAssetEntry>& Editor::FEditorAssetCatalog::GetEntries() const {
        return mEntries;
    }

    /**
     * @brief 표시 폴더들을 반환합니다.
     */
    const Core::TArray<std::filesystem::path>& Editor::FEditorAssetCatalog::GetDirectories() const {
        return mDirectories;
    }

    /**
     * @brief 실제 경로에 해당하는 항목을 조회합니다.
     */
    const Editor::FEditorAssetEntry* Editor::FEditorAssetCatalog::Find(const std::filesystem::path& Path) const {
        Core::FString Error{};
        const Core::FString Relative{mAssets != nullptr ? mAssets->GetRelativePath(Path, Error) : Core::FString{}};
        const auto Position{mIndices.find(MakeKey(Core::FilePath::FromUtf8(Relative)))};

        return Position == mIndices.end() ? nullptr : &mEntries[Position->second];
    }

    /**
     * @brief 프로젝트 에셋 관리자를 반환합니다.
     */
    Asset::FAssetManager* Editor::FEditorAssetCatalog::GetAssets() const {
        return mAssets;
    }

}
