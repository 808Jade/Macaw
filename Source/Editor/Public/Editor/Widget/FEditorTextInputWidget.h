#pragma once

#include "Editor/Widget/FEditorWidget.h"

#include <string_view>
#include <vector>

namespace Editor {
    class FEditorTextInputWidget final : public Editor::FEditorWidget {
    public:
        FEditorTextInputWidget(Core::FString Id, Core::FString Label, Core::FString Hint = {}, std::function<Core::FString()> Getter = {}, std::function<void(const Core::FString&)> Setter = {}, Index_t Capacity = 1024);
        ~FEditorTextInputWidget() override;

    public:
        std::string_view GetText() const;
        void SetText(std::string_view Text);
        void SetCommittedHandler(std::function<void(const Core::FString&)> Handler);
        FVector2 ComputeDesiredSize() const override;

    private:
        void DrawContent(float DeltaTime) override;
        void AfterDraw() override;

    private:
        Core::FString mLabel{};
        Core::FString mHint{};
        std::function<Core::FString()> mGetter{};
        std::function<void(const Core::FString&)> mSetter{};
        std::function<void(const Core::FString&)> mCommitted{};
        std::vector<char> mBuffer{};
        bool mChanged{};
        bool mSubmitted{};
    };
}
