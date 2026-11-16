#pragma once

#include "Editor/Widget/FEditorWidget.h"

namespace Editor {
    class FEditorContentWidget final : public Editor::FEditorWidget {
    public:
        FEditorContentWidget(Core::FString Id, std::function<void(float)> Content, std::function<FVector2()> DesiredSize = {}, bool ContentRegion = true);
        ~FEditorContentWidget() override;

    public:
        FVector2 ComputeDesiredSize() const override;
        bool UsesContentRegion() const override;

    private:
        void DrawContent(float DeltaTime) override;

    private:
        std::function<void(float)> mContent{};
        std::function<FVector2()> mDesiredSize{};
        bool mContentRegion{};
    };
}
