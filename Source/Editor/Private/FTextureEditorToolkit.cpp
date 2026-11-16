/**
 * @file FTextureEditorToolkit.cpp
 * @brief 텍스처 정보를 표시하고 이미지 미리 보기를 제공하는 에셋 툴킷입니다.
 * @ingroup Editor
 */

#include "FTextureEditorToolkit.h"
#include "Editor/FEditorToolkitHost.h"
#include "Editor/Panel/FEditorPanel.h"
#include "Editor/Widget/FEditorContentWidget.h"
#include "Editor/FEditorContext.h"
#include "EditorStyle.h"
#include "Core/FilePath.h"
#include "Engine/FProject.h"
#include "Asset/UTexture2D.h"
#include "Viewer/FAssetViewer.h"
#include <imgui.h>

using namespace Editor;

FTextureEditorToolkit::FTextureEditorToolkit(FEditorContext* Context, const std::filesystem::path& Path, Asset::UTexture2D* Texture)
	: FAssetEditorToolkit{Path, Asset::EAssetType::Texture, "###Texture Preview:" + Core::FilePath::ToGenericUtf8(Path)},
	  mContext{Context},
	  mPreviewTexture{Texture} {
    GetTab().SetIcon(EEditorIcon::Sprite, IM_COL32(220, 80, 80, 255));
    GetToolkitHost().AddWidget("Preview", "Preview", [this](float) {
        DrawPreview();
    });
    auto Details{std::make_unique<FEditorPanel>("TextureDetails")};
    Details->AddChild(std::make_unique<FEditorContentWidget>("AssetPath", [this](float) {
        ImGui::TextUnformatted(Core::FilePath::ToGenericUtf8(GetAssetPath()).c_str());
    }, []() {
        return FVector2{0.0f, ImGui::GetTextLineHeightWithSpacing()};
    }), EEditorWidgetSizeRule::Auto);
    Details->AddChild(std::make_unique<FEditorContentWidget>("Dimensions", [this](float) {
        if (mPreviewTexture != nullptr) {
            ImGui::Text("Size: %u x %u", mPreviewTexture->GetSizeX(), mPreviewTexture->GetSizeY());
        }
    }, []() {
        return FVector2{0.0f, ImGui::GetTextLineHeightWithSpacing()};
    }), EEditorWidgetSizeRule::Auto);
    GetToolkitHost().AddWidget("Details", "Details", std::move(Details), EEditorDockRegion::Details);
}

FTextureEditorToolkit::~FTextureEditorToolkit() = default;

void FTextureEditorToolkit::UpdateEditor(float) {
    GetTab().SetName(EditorStyle::TabIconPadding + Core::FilePath::ToUtf8(GetAssetPath().stem()) + "###Texture Preview:" + Core::FilePath::ToGenericUtf8(GetAssetPath()));

    if (mPreviewTexture == nullptr || !mPreviewTexture->HasResources()) {
        mPreviewTexture = nullptr;
        GetTab().SetVisible(false);
    }
}

void FTextureEditorToolkit::DrawPreview() {
    if (mPreviewTexture == nullptr) {
        return;
    }

    const ImVec2 Available{ImGui::GetContentRegionAvail()};

    if (Available.x > 0.0f && Available.y > 0.0f) {
        if (mTextureViewer == nullptr) {
            mTextureViewer = std::make_unique<Viewer::FAssetViewer>(mContext->GetRenderer(), mContext->GetProject()->GetAssets());
        }

        if (mTextureViewer->GetContent() == nullptr) {
            mTextureViewer->SetAsset(mPreviewTexture, Asset::EAssetType::Texture);
        }

        mTextureViewer->Resize(static_cast<Uint32>(Available.x), static_cast<Uint32>(Available.y));
        mTextureViewer->Fit(1.0f);
        const FVector4 Rect{mTextureViewer->GetImageRect()};
        const ImVec2 Origin{ImGui::GetCursorPos()};

        ImGui::SetCursorPos({Origin.x + Rect.x, Origin.y + Rect.y});
        ImGui::Image(ImTextureRef{static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mTextureViewer->GetNativeTexture()))}, {Rect.z, Rect.w});
    }
}
