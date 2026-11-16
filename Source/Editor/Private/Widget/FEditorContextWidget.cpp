#include "Editor/Widget/FEditorContextWidget.h"

#include <utility>

using namespace Editor;

FEditorContextWidget::FEditorContextWidget(FEditorContext* Context, Core::FString Id)
	: FEditorWidget{std::move(Id)},
	  mContext{Context} {
}

FEditorContextWidget::~FEditorContextWidget() = default;

FEditorContext* FEditorContextWidget::GetEditorContext() const {
    return mContext;
}

void FEditorContextWidget::SetEditorContext(FEditorContext* Context) {
    if (Context != nullptr) {
        mContext = Context;
    }
}

bool FEditorContextWidget::UsesContentRegion() const {
    return true;
}
