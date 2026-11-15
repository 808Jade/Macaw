/**
 * @file FProjectTemplatePicker.cpp
 * @brief 프로젝트 템플릿 목록과 미리보기 이미지를 읽고 선택 UI를 제공합니다.
 * @ingroup Editor
 */

#include "Core/Types.h"
#include "Editor/FProjectTemplatePicker.h"
#include "Asset/UTexture2D.h"
#include "Core/FilePath.h"
#include "Serialization/JsonArchive.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace Editor {

    FProjectTemplatePicker::FProjectTemplatePicker() = default;

    FProjectTemplatePicker::~FProjectTemplatePicker() = default;

    bool FProjectTemplatePicker::Load(const std::filesystem::path& Directory, const std::filesystem::path& SelectedDirectory, Core::FString& Error) {
        const auto Preferred{SelectedDirectory.lexically_normal()};
        mTemplates.clear();
        mSelectedIndex = 0;
        Error.clear();
        std::error_code Code{};
        std::filesystem::directory_iterator Entry{Directory, Code};

        if (Code) {
            Error = "Cannot find the project templates: " + Code.message();

            return false;
        }

        const std::filesystem::directory_iterator End{};

        while (Entry != End && !Code) {
            const auto Path{Entry->path()};

            if (Entry->is_directory(Code) && std::filesystem::exists(Path / "Template.mproject", Code) && !LoadTemplate(Path, Error)) {
                return false;
            }

            if (Code) {
                break;
            }

            Entry.increment(Code);
        }

        if (Code || mTemplates.empty()) {
            Error = Code ? "Cannot read the project templates: " + Code.message() : "No project templates are installed";

            return false;
        }

        std::sort(mTemplates.begin(), mTemplates.end(), [](const auto& A, const auto& B) {
            if ((A.mId == "Default") != (B.mId == "Default")) {
                return A.mId == "Default";
            }

            return A.mName < B.mName;
        });

        for (Core::Index_t Index{}; Index < mTemplates.size(); ++Index) {
            if (mTemplates[Index].mDirectory.lexically_normal() == Preferred) {
                mSelectedIndex = Index;
            }
        }

        return true;
    }

    bool FProjectTemplatePicker::LoadTemplate(const std::filesystem::path& Directory, Core::FString& Error) {
        FTemplateInfo Template{};
        Template.mDirectory = Directory;
        Template.mId = Core::FilePath::ToUtf8(Directory.filename());
        Template.mName = Template.mId;
        Template.mDescription = "Create a project from this template.";
        const auto Manifest{Directory / "Template.json"};
        std::error_code Code{};

        if (std::filesystem::is_regular_file(Manifest, Code)) {
            Serialization::FJsonArchive Archive{Core::EArchiveMode::Loading};
            Core::FString Preview{};

            if (!Archive.Load(Manifest)) {
                Error = Archive.GetError();

                return false;
            }

            Archive.Serialize("Id", Template.mId);
            Archive.Serialize("Name", Template.mName);
            Archive.Serialize("Description", Template.mDescription);

            if (Archive.HasField("RequiresScripts")) {
                Archive.Serialize("RequiresScripts", Template.mRequiresScripts);
            }

            if (Archive.HasField("Preview")) {
                Archive.Serialize("Preview", Preview);
            }

            const std::filesystem::path PreviewPath{Core::FilePath::FromUtf8(Preview)};
            const bool InvalidPreview{PreviewPath.has_root_path() || std::any_of(PreviewPath.begin(), PreviewPath.end(), [](const auto& Part) {
                return Part == "..";
            })};

            if (Archive.HasError() || Template.mId.empty() || Template.mName.empty() || InvalidPreview) {
                Error = Archive.HasError() ? Archive.GetError() : "Invalid project template metadata";

                return false;
            }

            if (!Preview.empty()) {
                Template.mPreviewPath = Directory / PreviewPath;
            }
        }

        if (std::any_of(mTemplates.begin(), mTemplates.end(), [&Template](const auto& Existing) {
            return Existing.mId == Template.mId;
        })) {
            Error = "Duplicate project template identifier: " + Template.mId;

            return false;
        }

        mTemplates.push_back(std::move(Template));

        return true;
    }

    bool FProjectTemplatePicker::SelectTemplate(const Core::FString& Id) {
        const auto Found{std::find_if(mTemplates.begin(), mTemplates.end(), [&Id](const auto& Template) {
            return Template.mId == Id;
        })};

        if (Found == mTemplates.end()) {
            return false;
        }

        mSelectedIndex = static_cast<Core::Index_t>(Found - mTemplates.begin());

        return true;
    }

    const std::filesystem::path& FProjectTemplatePicker::GetSelectedDirectory() const {
        static const std::filesystem::path Empty{};

        return mSelectedIndex < mTemplates.size() ? mTemplates[mSelectedIndex].mDirectory : Empty;
    }

    Core::Uint32 FProjectTemplatePicker::GetTemplateCount() const {
        return static_cast<Core::Uint32>(mTemplates.size());
    }

    void FProjectTemplatePicker::LoadPreview(FTemplateInfo& Template, void* Device) {
        if (Device == nullptr || Template.mPreviewAttempted || Template.mPreviewPath.empty()) {
            return;
        }

        Template.mPreviewAttempted = true;
        auto Texture{std::make_unique<Asset::UTexture2D>()};

        if (Texture->LoadFromFile(Template.mPreviewPath, false) && Texture->CreateResources(static_cast<ID3D11Device*>(Device))) {
            Template.mPreview = std::move(Texture);
        }
    }

    namespace {

        void DrawTemplateImage(ImDrawList* Draw, Asset::UTexture2D* Texture, ImVec2 Min, ImVec2 Max) {
            if (Texture != nullptr) {
                const float Aspect{static_cast<float>(Texture->GetSizeX()) / static_cast<float>(std::max(Texture->GetSizeY(), 1u))};
                const float TargetAspect{(Max.x - Min.x) / (Max.y - Min.y)};
                const ImVec2 Crop{Aspect > TargetAspect ? TargetAspect / Aspect : 1.0f, Aspect > TargetAspect ? 1.0f : Aspect / TargetAspect};
                Draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(Texture->GetShaderResourceView())), Min, Max, {(1.0f - Crop.x) * 0.5f, (1.0f - Crop.y) * 0.5f}, {(1.0f + Crop.x) * 0.5f, (1.0f + Crop.y) * 0.5f});

                return;
            }

            Draw->AddRectFilledMultiColor(Min, Max, IM_COL32(35, 47, 57, 255), IM_COL32(55, 71, 80, 255), IM_COL32(29, 34, 42, 255), IM_COL32(29, 34, 42, 255));
            const ImVec2 Center{(Min.x + Max.x) * 0.5f, Min.y + (Max.y - Min.y) * 0.43f};
            const float W{(Max.y - Min.y) * 0.24f};
            const float H{W * 0.58f};

            for (int Index{-4}; Index <= 4; ++Index) {
                const float Offset{static_cast<float>(Index) * W * 0.65f};
                Draw->AddLine({Center.x - W * 4.0f + Offset, Max.y}, {Center.x + Offset, Center.y + H}, IM_COL32(66, 77, 87, 100));
                Draw->AddLine({Center.x + W * 4.0f + Offset, Max.y}, {Center.x + Offset, Center.y + H}, IM_COL32(66, 77, 87, 100));
            }

            Draw->AddQuadFilled({Center.x, Center.y - H}, {Center.x + W, Center.y - H * 0.12f}, {Center.x, Center.y + H * 0.8f}, {Center.x - W, Center.y - H * 0.12f}, IM_COL32(207, 214, 221, 255));
            Draw->AddQuadFilled({Center.x - W, Center.y - H * 0.12f}, {Center.x, Center.y + H * 0.8f}, {Center.x, Center.y + H * 2.6f}, {Center.x - W, Center.y + H * 1.7f}, IM_COL32(110, 130, 147, 255));
            Draw->AddQuadFilled({Center.x, Center.y + H * 0.8f}, {Center.x + W, Center.y - H * 0.12f}, {Center.x + W, Center.y + H * 1.7f}, {Center.x, Center.y + H * 2.6f}, IM_COL32(66, 87, 104, 255));
        }
    }

    bool FProjectTemplatePicker::Draw(void* Device) {
        if (mTemplates.empty()) {
            return false;
        }

        const float Scale{ImGui::GetFontSize() / 16.0f};
        const float Width{142.0f * Scale};
        const float Height{190.0f * Scale};
        const float Gap{14.0f * Scale};
        const int Columns{std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + Gap) / (Width + Gap)))};
        const ImVec2 Start{ImGui::GetCursorPos()};
        bool Changed{};

        for (Core::Index_t Index{}; Index < mTemplates.size(); ++Index) {
            auto& Template{mTemplates[Index]};
            ImGui::PushID(Template.mId.c_str());
            ImGui::SetCursorPos({Start.x + static_cast<float>(Index % Columns) * (Width + Gap) + 2.0f, Start.y + static_cast<float>(Index / Columns) * (Height + Gap) + 2.0f});
            const ImVec2 Min{ImGui::GetCursorScreenPos()};
            const ImVec2 Max{Min.x + Width, Min.y + Height};

            if (ImGui::Selectable("##Template", mSelectedIndex == Index, ImGuiSelectableFlags_None, {Width, Height})) {
                mSelectedIndex = Index;
                Changed = true;
            }

            const bool Hovered{ImGui::IsItemHovered()};
            ImDrawList* Draw{ImGui::GetWindowDrawList()};
            const bool Selected{mSelectedIndex == Index};
            Draw->AddRectFilled(Min, Max, Selected ? IM_COL32(0, 122, 218, 255) : IM_COL32(48, 48, 50, 255), 2.0f * Scale);
            LoadPreview(Template, Device);
            DrawTemplateImage(Draw, Template.mPreview.get(), {Min.x + 2.0f, Min.y + 2.0f}, {Max.x - 2.0f, Min.y + Width - 2.0f});
            Draw->AddText(ImGui::GetFont(), 15.0f * Scale, {Min.x + 10.0f * Scale, Min.y + Width + 8.0f * Scale}, IM_COL32(242, 243, 246, 255), Template.mName.c_str());
            Draw->AddText(ImGui::GetFont(), 11.0f * Scale, {Min.x + 10.0f * Scale, Max.y - 16.0f * Scale}, Selected ? IM_COL32(210, 233, 255, 255) : IM_COL32(155, 159, 166, 255), Template.mRequiresScripts ? "C# GAMEPLAY" : "NATIVE SCENE");
            Draw->AddRect(Min, Max, Selected ? IM_COL32(0, 145, 246, 255) : Hovered ? IM_COL32(126, 134, 143, 255) : IM_COL32(60, 60, 63, 255), 2.0f, 0, Selected ? 2.0f : 1.0f);
            ImGui::PopID();
        }

        return Changed;
    }

    void FProjectTemplatePicker::DrawSelectedPreview(void* Device, float Width, float Height) {
        if (mSelectedIndex >= mTemplates.size()) {
            return;
        }

        auto& Template{mTemplates[mSelectedIndex]};
        LoadPreview(Template, Device);
        const ImVec2 Min{ImGui::GetCursorScreenPos()};
        DrawTemplateImage(ImGui::GetWindowDrawList(), Template.mPreview.get(), Min, {Min.x + Width, Min.y + Height});
        ImGui::Dummy({Width, Height});
    }

    const Core::FString& FProjectTemplatePicker::GetSelectedName() const {
        static const Core::FString Empty{};

        return mSelectedIndex < mTemplates.size() ? mTemplates[mSelectedIndex].mName : Empty;
    }

    const Core::FString& FProjectTemplatePicker::GetSelectedDescription() const {
        static const Core::FString Empty{};

        return mSelectedIndex < mTemplates.size() ? mTemplates[mSelectedIndex].mDescription : Empty;
    }

    bool FProjectTemplatePicker::SelectedRequiresScripts() const {
        return mSelectedIndex < mTemplates.size() && mTemplates[mSelectedIndex].mRequiresScripts;
    }

    void FProjectTemplatePicker::ReleasePreviews() {
        for (auto& Template : mTemplates) {
            Template.mPreview.reset();
            Template.mPreviewAttempted = false;
        }
    }
}
