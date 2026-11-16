/**
 * @file FMaterialEditorToolkit.cpp
 * @brief 머티리얼 속성·미리 보기·저장과 변경 확인을 제공하는 에셋 툴킷입니다.
 * @ingroup Editor
 */

#include "FMaterialEditorToolkit.h"
#include "Editor/FEditorToolkitHost.h"
#include "Core/FilePath.h"
#include "EditorStyle.h"
#include "Editor/FEditorContext.h"
#include "Editor/FEditorAssetCatalog.h"
#include "Editor/FAssetThumbnailCache.h"
#include "Engine/FProject.h"
#include "Asset/UMaterial.h"
#include "CoreUObject/Reflection/NewObject.h"
#include "Renderer/Renderer.h"
#include "Platform/Window.h"
#include <imgui.h>
#include <algorithm>

using namespace Editor;

FMaterialEditorToolkit::FMaterialEditorToolkit(FEditorContext* Context, Platform::FWindowsApplication* Platform, const std::filesystem::path& Path, Asset::UMaterial* Material, FEditorAssetCatalog& Catalog, FAssetThumbnailCache& Thumbnails)
	: FAssetEditorToolkit{Path, Asset::EAssetType::Material, "###MaterialEditor:" + Core::FilePath::ToGenericUtf8(Path)},
	  mContext{Context},
	  mPlatform{Platform},
	  mEditingMaterial{Material},
	  mAssetCatalog{&Catalog},
	  mThumbnails{&Thumbnails} {
    GetTab().SetIcon(EEditorIcon::Material, IM_COL32(64, 192, 64, 255));
    GetToolkitHost().AddWidget("Preview", "Preview", [this](float) {
        DrawPreview();
    });
    GetToolkitHost().AddWidget("Details", "Details", [this](float) {
        DrawDetails();
    },
        EEditorDockRegion::Details);
    GetTab().SetCanCloseHandler([this]() {
        return ConfirmMaterialChange();
    });
}

FMaterialEditorToolkit::~FMaterialEditorToolkit() = default;

bool FMaterialEditorToolkit::CanClose() {
    return ConfirmMaterialChange();
}

void FMaterialEditorToolkit::UpdateEditor(float) {
    if (mEditingMaterial != nullptr) {
        mThumbnails->GetMaterialPreview(mEditingMaterial);
    }

    GetTab().SetName(EditorStyle::TabIconPadding + Core::FilePath::ToUtf8(GetAssetPath().stem()) + (mMaterialDirty ? "*" : "") + "###MaterialEditor:" + Core::FilePath::ToGenericUtf8(GetAssetPath()));
}

bool FMaterialEditorToolkit::SaveMaterial() {
    if (mEditingMaterial == nullptr || !mContext->GetProject()->GetAssets()->SaveMaterial(GetAssetPath(), *mEditingMaterial, mMaterialError)) {
        return false;
    }

    mMaterialDirty = false;
    NotifyChanged();
    mContext->AddLog("Saved " + Core::FilePath::ToUtf8(GetAssetPath()));

    return true;
}

bool FMaterialEditorToolkit::ConfirmMaterialChange() {
    if (!mMaterialDirty || mEditingMaterial == nullptr) {
        return true;
    }

    const Int32 Choice{MessageBoxW(mPlatform->GetWindowHandle(), L"Save changes to the current material?", L"Material", MB_YESNOCANCEL | MB_ICONQUESTION)};

    if (Choice == IDCANCEL) {
        return false;
    }

    if (Choice == IDYES) {
        return SaveMaterial();
    }

    if (!mContext->GetProject()->GetAssets()->ReloadMaterial(GetAssetPath(), mMaterialError)) {
        return false;
    }

    mMaterialDirty = false;

    return true;
}

bool FMaterialEditorToolkit::SetMaterialTexture(const Core::FString& Path) {
    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};

    if (!Path.empty() && Assets->LoadTexture(Core::FilePath::FromUtf8(Path), mMaterialError) == nullptr) {
        return false;
    }

    if (!mEditingMaterial->SetBaseColorTexturePath(Path) || !mEditingMaterial->ResolveTexture(*Assets, mMaterialError)) {
        if (mMaterialError.empty()) {
            mMaterialError = "Invalid texture path";
        }

        return false;
    }

    mMaterialError.clear();
    mMaterialDirty = true;

    return true;
}

bool FMaterialEditorToolkit::SetMaterialShader(const Core::FString& Path) {
    Asset::FAssetManager* Assets{mContext->GetProject()->GetAssets()};
    RenderCore::UShader* Shader{Path.empty() ? RenderCore::UShader::GetDefault() : RenderCore::UShader::FindBuiltin(Path)};

    if (Shader == nullptr) {
        Shader = Assets->LoadShader(Core::FilePath::FromUtf8(Path), mMaterialError);
    }

    if (Shader == nullptr) {
        return false;
    }

    auto Candidate{CoreUObject::NewObject<Asset::UMaterial>()};
    Candidate->SetTwoSided(mEditingMaterial->IsTwoSided());

    Renderer::FRenderer* Renderer{dynamic_cast<Renderer::FRenderer*>(mContext->GetRenderer())};

    if (!Candidate->SetShader(Shader) || Renderer == nullptr || Renderer->GetMaterialPipeline(*Candidate) == nullptr) {
        mMaterialError = "Cannot use this material shader. Check the shader diagnostics in the log.";

        return false;
    }

    mEditingMaterial->SetShader(Shader);
    mMaterialError.clear();
    mMaterialDirty = true;

    return true;
}

void FMaterialEditorToolkit::HandleCommands() {
    if (mEditingMaterial != nullptr && mMaterialDirty && mContext->CanEditWorld() && GetTab().IsFocused() && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
        SaveMaterial();
    }
}

void FMaterialEditorToolkit::DrawPreview() {
    if (mEditingMaterial == nullptr) {
        return;
    }

    if (void* Preview{mThumbnails->GetMaterialPreview(mEditingMaterial)}) {
        const ImVec2 Available{ImGui::GetContentRegionAvail()};
        const float Size{std::max(1.0f, std::min(Available.x, Available.y))};
        ImGui::Image(ImTextureRef{static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(Preview))}, {Size, Size});
    }
}

void FMaterialEditorToolkit::DrawDetails() {
    if (mEditingMaterial == nullptr) {
        return;
    }

    ImGui::TextUnformatted(Core::FilePath::ToGenericUtf8(GetAssetPath()).c_str());
    ImGui::BeginDisabled(!mContext->CanEditWorld());

    if (EditorStyle::Button("Reload") && ConfirmMaterialChange()) {
        mContext->GetProject()->GetAssets()->ReloadMaterial(GetAssetPath(), mMaterialError);
    }

    ImGui::Separator();

    const RenderCore::UShader* Shader{mEditingMaterial->GetShader().Get()};

    if (ImGui::BeginCombo("Builtin Shader", Shader != nullptr ? Shader->GetName().c_str() : "Missing")) {
        for (RenderCore::UShader* Builtin : {RenderCore::UShader::GetDefault(), RenderCore::UShader::GetUnlit()}) {
            if (ImGui::Selectable(Builtin->GetName().c_str(), Shader == Builtin)) {
                SetMaterialShader(Builtin->GetSourcePath());
            }
        }

        ImGui::EndCombo();
    }

    Core::FString ShaderPath{mContext->GetProject()->GetAssets()->GetShaderPath(Shader)};

    if (mAssetPicker.Draw(*mAssetCatalog, *mThumbnails, "Shader Asset", ShaderPath, Asset::EAssetType::Shader)) {
        SetMaterialShader(ShaderPath);
    }

    auto Color{mEditingMaterial->GetBaseColor().ToArray()};

    if (ImGui::ColorEdit4("Base Color", Color.data()) && mEditingMaterial->SetBaseColor({Color[0], Color[1], Color[2], Color[3]})) {
        mMaterialDirty = true;
    }

    int BlendMode{static_cast<int>(mEditingMaterial->GetBlendMode())};

    if (ImGui::Combo("Blend Mode", &BlendMode, "Opaque\0Translucent\0") && mEditingMaterial->SetBlendMode(static_cast<Asset::EBlendMode>(BlendMode))) {
        mMaterialDirty = true;
    }

    bool TwoSided{mEditingMaterial->IsTwoSided()};

    if (ImGui::Checkbox("Two Sided", &TwoSided)) {
        mEditingMaterial->SetTwoSided(TwoSided);
        mMaterialDirty = true;
    }

    Core::FString TexturePath{mEditingMaterial->GetBaseColorTexturePath()};

    if (mAssetPicker.Draw(*mAssetCatalog, *mThumbnails, "Base Color Texture", TexturePath, Asset::EAssetType::Texture)) {
        SetMaterialTexture(TexturePath);
    }

    Core::FString NormalPath{mEditingMaterial->GetNormalMapTexturePath()};

    if (mAssetPicker.Draw(*mAssetCatalog, *mThumbnails, "Normal Map", NormalPath, Asset::EAssetType::Texture)) {
        const Core::FString Previous{mEditingMaterial->GetNormalMapTexturePath()};

        if (mEditingMaterial->SetNormalMapTexturePath(NormalPath) && mEditingMaterial->ResolveTexture(*mContext->GetProject()->GetAssets(), mMaterialError)) {
            mMaterialDirty = true;
        } else {
            mEditingMaterial->SetNormalMapTexturePath(Previous);

            Core::FString RestoreError{};

            mEditingMaterial->ResolveTexture(*mContext->GetProject()->GetAssets(), RestoreError);
        }
    }

    float Specular{mEditingMaterial->GetSpecularStrength()};
    float Shininess{mEditingMaterial->GetShininess()};
    float NormalStrength{mEditingMaterial->GetNormalStrength()};

    if (ImGui::SliderFloat("Specular Strength", &Specular, 0.0f, 1.0f) && mEditingMaterial->SetSpecularStrength(Specular)) {
        mMaterialDirty = true;
    }

    if (ImGui::SliderFloat("Shininess", &Shininess, 1.0f, 1024.0f, "%.1f", ImGuiSliderFlags_Logarithmic) && mEditingMaterial->SetShininess(Shininess)) {
        mMaterialDirty = true;
    }

    if (ImGui::SliderFloat("Normal Strength", &NormalStrength, 0.0f, 8.0f) && mEditingMaterial->SetNormalStrength(NormalStrength)) {
        mMaterialDirty = true;
    }

    ImGui::EndDisabled();

    if (!mMaterialError.empty()) {
        ImGui::TextWrapped("%s", mMaterialError.c_str());
    }
}
