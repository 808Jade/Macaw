#include "Editor/Widget/FEditorContentWidget.h"

#include <utility>

using namespace Editor;

FEditorContentWidget::FEditorContentWidget(Core::FString Id, std::function<void(float)> Content, std::function<FVector2()> DesiredSize, bool ContentRegion)
	: FEditorWidget{std::move(Id)},
	  mContent{std::move(Content)},
	  mDesiredSize{std::move(DesiredSize)},
	  mContentRegion{ContentRegion} {
}

FEditorContentWidget::~FEditorContentWidget() = default;

FVector2 FEditorContentWidget::ComputeDesiredSize() const {
    return mDesiredSize ? mDesiredSize() : FVector2{};
}

bool FEditorContentWidget::UsesContentRegion() const {
    return mContentRegion;
}

void FEditorContentWidget::DrawContent(float DeltaTime) {
    if (mContent) {
        mContent(DeltaTime);
    }
}
