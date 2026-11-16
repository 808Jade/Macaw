#pragma once
#include "Core/Types.h"
#include "Core/Math/FVector2.h"

#include <functional>
#include <memory>

namespace Editor {
    class FEditorPanel;

    class FEditorWidget {
        friend class Editor::FEditorPanel;

    public:
        explicit FEditorWidget(Core::FString Id);
        virtual ~FEditorWidget();
        FEditorWidget(const Editor::FEditorWidget&) = delete;
        Editor::FEditorWidget& operator=(const Editor::FEditorWidget&) = delete;

    public:
        void Draw(float DeltaTime);
        void Draw(float DeltaTime, const FVector2& Size);
        virtual FVector2 ComputeDesiredSize() const;
        virtual bool UsesContentRegion() const;
        const FVector2& GetAllocatedSize() const;
        bool HasAllocatedSize() const;

        const Core::FString& GetId() const;
        Editor::FEditorPanel* GetParent() const;
        bool IsVisible() const;
        void SetVisible(bool Visible);
        bool IsEnabled() const;
        void SetEnabled(bool Enabled);
        void SetEnabledAttribute(std::function<bool()> Attribute);
        void SetToolTip(Core::FString ToolTip);

        bool AttachTo(const void* Owner);
        void DetachFrom(const void* Owner);
        bool IsAttached() const;
        std::weak_ptr<void> GetLifetime();
        void NotifyHidden();

    private:
        virtual void DrawContent(float DeltaTime) = 0;
        virtual void AfterDraw();
        virtual void OnHidden();

    private:
        Core::FString mId{};
        Editor::FEditorPanel* mParent{};
        const void* mOwner{};
        std::shared_ptr<void> mLifetime{};
        std::function<bool()> mEnabledAttribute{};
        Core::FString mToolTip{};
        FVector2 mAllocatedSize{};
        bool mHasAllocatedSize{};
        bool mVisible{true};
        bool mEnabled{true};
    };
}
