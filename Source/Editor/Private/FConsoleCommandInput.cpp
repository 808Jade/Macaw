/**
 * @file FConsoleCommandInput.cpp
 * @brief 명령 실행과 자동 완성 후보 및 입력 기록을 제공하는 콘솔 입력 위젯입니다.
 * @ingroup Editor
 */

#include "Editor/FConsoleCommandInput.h"
#include "Editor/FEditorContext.h"
#include "EditorStyle.h"
#include "FEditorAnimation.h"

#include <imgui_internal.h>
#include <algorithm>

using namespace Editor;

FConsoleCommandInput::FConsoleCommandInput(FEditorContext* Context)
	: mContext{Context} {
}

FConsoleCommandInput::~FConsoleCommandInput() = default;

void FConsoleCommandInput::Draw(const char* Id, float Width, bool ShowRunButton) {
    ImGui::PushID(Id);

    const ImGuiStyle& Style{ImGui::GetStyle()};
    const float Available{Width > 0.0f ? Width : ImGui::GetContentRegionAvail().x};
    const float Start{ImGui::GetCursorPosX()};
    const float RunWidth{ImGui::CalcTextSize("Run").x + Style.FramePadding.x * 2.0f};
    const bool ShowRun{ShowRunButton && Available >= RunWidth + Style.ItemSpacing.x + 6.0f * ImGui::GetFontSize()};
    bool RunClicked{};

    if (ShowRun) {
        ImGui::SetCursorPosX(Start + Available - RunWidth);
        ImGui::BeginDisabled(mInput.front() == '\0');
        RunClicked = EditorStyle::Button("Run", {RunWidth, 0.0f});
        ImGui::EndDisabled();
        ImGui::SameLine(Start, 0.0f);
    }

    if (mCompletionRequested || RunClicked) {
        ImGui::SetKeyboardFocusHere();
    }

    const Uint32 InputId{ImGui::GetID("##Command")};
    ImGuiWindow* Parent{ImGui::GetCurrentWindow()};
    ImGui::SetNextItemWidth(ShowRun ? Available - RunWidth - Style.ItemSpacing.x : Available);

    constexpr ImGuiInputTextFlags Flags{ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackAlways};
    const bool Enter{ImGui::InputTextWithHint("##Command", "Enter Console Command", mInput.data(), mInput.size(), Flags, HandleInput, this)};
    const bool Activated{ImGui::IsItemActivated()};
    mInputActive = ImGui::IsItemActive();
    const ImVec2 Minimum{ImGui::GetItemRectMin()};
    const ImVec2 Maximum{ImGui::GetItemRectMax()};
    if (mSuggestions.empty()) {
        ImGui::SetItemTooltip("실행: Enter | 자동 완성: Tab\n후보 선택: Ctrl+Up / Ctrl+Down | 명령 기록: Up / Down");
    }

    if (Enter || RunClicked) {
        const Core::FString Command{mInput.data()};

        if (Command.find_first_not_of(" \t\r\n\f\v") != Core::FString::npos) {
            mContext->ExecuteConsoleCommand(Command);

            if (mHistory.empty() || mHistory.back() != Command) {
                if (mHistory.size() >= 100) {
                    mHistory.erase(mHistory.begin());
                }

                mHistory.push_back(Command);
            }
        }

        mHistoryPosition = mHistory.size();
        mHistoryDraft.clear();
        mInput.fill('\0');
        mSuggestions.clear();
        ImGui::SetKeyboardFocusHere(-1);
    } else if (Activated && !mCompletionRequested && mHistoryPosition == mHistory.size()) {
        UpdateSuggestions(mInput.data());
    }

    if (!mSuggestions.empty() && (mInputActive || mCompletionRequested || (mPopupHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
        DrawSuggestions(InputId, Parent, {Minimum.x, Minimum.y}, {Maximum.x, Maximum.y});
    } else {
        mPopupHovered = false;
    }

    ImGui::PopID();
}

void FConsoleCommandInput::DrawSuggestions(Uint32 InputId, ImGuiWindow* Parent, const FVector2& Minimum, const FVector2& Maximum) {
    const ImGuiViewport* Viewport{ImGui::GetMainViewport()};
    const float RowHeight{ImGui::GetTextLineHeightWithSpacing()};
    const float Padding{ImGui::GetStyle().WindowPadding.y};
    const float Width{std::max(1.0f, std::min(std::max(Maximum.x - Minimum.x, 300.0f), Viewport->WorkSize.x))};
    const float Height{std::min(RowHeight * static_cast<float>(std::min(mSuggestions.size(), std::size_t{8})) + Padding * 2.0f, std::max(RowHeight, Minimum.y - Viewport->WorkPos.y - 4.0f))};
    const ImVec2 Position{std::clamp(Minimum.x, Viewport->WorkPos.x, Viewport->WorkPos.x + Viewport->WorkSize.x - Width), std::max(Viewport->WorkPos.y, Minimum.y - Height - 2.0f)};
    const Core::FString Name{"##ConsoleCandidates_" + std::to_string(InputId)};
    const float Opacity{FEditorAnimation::Appear(InputId)};
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * Opacity);
    ImGui::SetNextWindowPos(Position);
    ImGui::SetNextWindowSize({Width, Height});

    constexpr ImGuiWindowFlags Flags{ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs};
    mPopupHovered = ImGui::IsMouseHoveringRect(Position, {Position.x + Width, Position.y + Height}, false);

    if (ImGui::Begin(Name.c_str(), nullptr, Flags)) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        if (mPopupHovered && ImGui::GetIO().MouseWheel != 0.0f) {
            ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseWheel * RowHeight * 3.0f);
        }

        for (Index_t Index{}; Index < mSuggestions.size(); ++Index) {
            const ImVec2 Origin{ImGui::GetCursorScreenPos()};
            const ImVec2 End{Origin.x + ImGui::GetContentRegionAvail().x, Origin.y + RowHeight};
            const bool Hovered{mPopupHovered && ImGui::IsMouseHoveringRect(Origin, End, true)};

            EditorStyle::SelectionBackground(ImGui::GetID(mSuggestions[Index].c_str()), Origin, End, Index == mSelectedSuggestion, Hovered);

            ImGui::TextUnformatted(mSuggestions[Index].c_str());

            if (Hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                mSelectedSuggestion = Index;
                mCompletionRequested = true;
                ImGui::FocusWindow(Parent);
                ImGui::SetActiveID(InputId, Parent);
            }

            if (Index == mSelectedSuggestion && mScrollToSuggestion) {
                ImGui::SetScrollHereY();
            }
        }

        mScrollToSuggestion = false;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void FConsoleCommandInput::UpdateSuggestions(std::string_view Input) {
    mSuggestions = mContext->GetConsoleSuggestions(Input);
    mSelectedSuggestion = 0;
    mScrollToSuggestion = true;
}

bool FConsoleCommandInput::ReplaceInput(ImGuiInputTextCallbackData& Data, const Core::FString& Text) {
    if (Text.size() >= static_cast<std::size_t>(Data.BufSize)) {
        return false;
    }

    Data.DeleteChars(0, Data.BufTextLen);
    Data.InsertChars(0, Text.data(), Text.data() + Text.size());
    return true;
}

void FConsoleCommandInput::BrowseHistory(ImGuiInputTextCallbackData& Data) {
    if (Data.EventKey == ImGuiKey_UpArrow) {
        if (mHistoryPosition == 0) {
            return;
        }

        if (mHistoryPosition == mHistory.size()) {
            mHistoryDraft.assign(Data.Buf, static_cast<std::size_t>(Data.BufTextLen));
        }

        --mHistoryPosition;
    } else {
        if (mHistoryPosition == mHistory.size()) {
            return;
        }

        ++mHistoryPosition;
    }

    if (mHistoryPosition == mHistory.size()) {
        ReplaceInput(Data, mHistoryDraft);
        UpdateSuggestions({Data.Buf, static_cast<std::size_t>(Data.BufTextLen)});
    } else {
        ReplaceInput(Data, mHistory[mHistoryPosition]);
        mSuggestions.clear();
    }
}

int FConsoleCommandInput::HandleInput(ImGuiInputTextCallbackData* Data) {
    FConsoleCommandInput& Input{*static_cast<FConsoleCommandInput*>(Data->UserData)};
    const std::size_t Count{Input.mSuggestions.size()};

    if (Input.mCompletionRequested || Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        Input.mCompletionRequested = false;

        if (Count != 0 && ReplaceInput(*Data, Input.mSuggestions[Input.mSelectedSuggestion])) {
            Input.mHistoryPosition = Input.mHistory.size();
            Input.UpdateSuggestions({Data->Buf, static_cast<std::size_t>(Data->BufTextLen)});
        }
    } else if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (!ImGui::GetIO().KeyCtrl) {
            Input.BrowseHistory(*Data);
        } else if (Count != 0) {
            Input.mSelectedSuggestion = Data->EventKey == ImGuiKey_UpArrow ? (Input.mSelectedSuggestion + Count - 1) % Count : (Input.mSelectedSuggestion + 1) % Count;
            Input.mScrollToSuggestion = true;
        }
    } else if (Data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        Input.mHistoryPosition = Input.mHistory.size();
        Input.UpdateSuggestions({Data->Buf, static_cast<std::size_t>(Data->BufTextLen)});
    }

    return 0;
}
