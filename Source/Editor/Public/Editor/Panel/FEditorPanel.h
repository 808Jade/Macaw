/**
 * @file FEditorPanel.h
 * @brief 자식 위젯을 수평·수직·겹침 방식으로 배치하는 복합 위젯입니다.
 * @ingroup Editor
 */

#pragma once

#include "Editor/Widget/FEditorWidget.h"
#include "Core/Math/FVector4.h"

#include <memory>

namespace Editor {
    /**
     * @brief 자식 위젯의 수평·수직·겹침 배치 방식입니다.
     */
    enum class EEditorPanelLayout {
        /**
         * @brief 자식을 왼쪽에서 오른쪽으로 배치합니다.
         */
        Horizontal,

        /**
         * @brief 자식을 위에서 아래로 배치합니다.
         */
        Vertical,

        /**
         * @brief 자식을 같은 콘텐츠 영역 위에 겹쳐 배치합니다.
         */
        Overlay
    };

    /**
     * @brief 패널에서 자식의 주축 크기를 결정하는 규칙입니다.
     */
    enum class EEditorWidgetSizeRule {
        /**
         * @brief 위젯의 희망 크기를 사용합니다.
         */
        Auto,

        /**
         * @brief 지정한 고정 크기를 사용합니다.
         */
        Fixed,

        /**
         * @brief 남는 공간을 가중치에 따라 분배합니다.
         */
        Fill
    };

    /**
     * @brief 자식 위젯을 수평·수직·겹침 방식으로 배치하는 복합 위젯입니다.
     */
    class FEditorPanel : public Editor::FEditorWidget {
    public:
        /**
         * @brief 화면에 배치할 자식 위젯과 패널 기준 위치·크기를 나타냅니다.
         */
        struct FArrangement {
            /**
             * @brief 연결된 자식 콘텐츠 위젯의 비소유 주소입니다.
             */
            Editor::FEditorWidget* mWidget{};

            /**
             * @brief 부모 패널 원점 기준 자식 위젯의 배치 위치입니다.
             */
            FVector2 mPosition{};

            /**
             * @brief 배치한 자식 위젯의 가로·세로 크기입니다.
             */
            FVector2 mSize{};
        };

    private:
        /**
         * @brief 자식 위젯의 선택적 소유권과 크기 규칙·여백을 보관합니다.
         */
        struct FSlot {
            /**
             * @brief 관리자가 선택적으로 수명을 소유하는 위젯입니다.
             */
            std::unique_ptr<Editor::FEditorWidget> mOwnedWidget{};

            /**
             * @brief 부모 패널에 연결된 자식 위젯의 비소유 주소입니다.
             */
            Editor::FEditorWidget* mWidget{};

            /**
             * @brief 자식의 자동·고정·공간 채움 크기 결정 방식입니다.
             */
            Editor::EEditorWidgetSizeRule mSizeRule{Editor::EEditorWidgetSizeRule::Fill};

            /**
             * @brief 고정 크기 또는 남는 공간을 분배할 가중치입니다.
             */
            float mExtent{1.0f};

            /**
             * @brief 자식 콘텐츠의 왼쪽·위·오른쪽·아래 여백입니다.
             */
            FVector4 mPadding{};
        };

    public:
        /**
         * @brief FEditorPanel의 초기 상태와 전달받은 의존성을 구성합니다.
         * @param[in] Layout 자식 위젯을 배치할 방식입니다.
         */
        FEditorPanel(Core::FString Id, Editor::EEditorPanelLayout Layout = Editor::EEditorPanelLayout::Vertical);

        /**
         * @brief 객체를 소멸하고 소유 멤버와 연결 상태를 정리합니다.
         */
        ~FEditorPanel() override;

    public:
        /**
         * @brief 부모가 없는 위젯을 크기 규칙과 여백을 지정해 자식으로 추가합니다.
         * @param[in] Widget 연결하거나 등록할 콘텐츠 위젯입니다.
         * @param[in] SizeRule 자식의 자동·고정·공간 채움 크기 규칙입니다.
         * @param[in] Extent 고정 크기 또는 공간 채움 가중치입니다.
         * @param[in] Padding 자식의 왼쪽·위·오른쪽·아래 내부 여백입니다.
         * @return 추가한 슬롯 인덱스이며 등록할 수 없으면 InvalidIndex입니다.
         */
        Index_t AddChild(Editor::FEditorWidget& Widget, Editor::EEditorWidgetSizeRule SizeRule = Editor::EEditorWidgetSizeRule::Fill, float Extent = 1.0f, const FVector4& Padding = {});

        /**
         * @brief 부모가 없는 위젯을 크기 규칙과 여백을 지정해 자식으로 추가합니다.
         * @param[in] Widget 연결하거나 등록할 콘텐츠 위젯입니다.
         * @param[in] SizeRule 자식의 자동·고정·공간 채움 크기 규칙입니다.
         * @param[in] Extent 고정 크기 또는 공간 채움 가중치입니다.
         * @param[in] Padding 자식의 왼쪽·위·오른쪽·아래 내부 여백입니다.
         * @return 추가한 슬롯 인덱스이며 등록할 수 없으면 InvalidIndex입니다.
         */
        Index_t AddChild(std::unique_ptr<Editor::FEditorWidget> Widget, Editor::EEditorWidgetSizeRule SizeRule = Editor::EEditorWidgetSizeRule::Fill, float Extent = 1.0f, const FVector4& Padding = {});

        /**
         * @brief 자식 위젯의 부모 연결을 해제하고 슬롯을 제거합니다.
         * @param[in] Widget 연결하거나 등록할 콘텐츠 위젯입니다.
         * @return 대상 위젯을 찾아 제거했으면 true입니다.
         */
        bool RemoveChild(Editor::FEditorWidget& Widget);

        /**
         * @brief 모든 자식의 부모 연결을 해제하고 소유 슬롯을 비웁니다.
         */
        void ClearChildren();

        /**
         * @brief 등록된 자식 슬롯 수를 반환합니다.
         * @return 현재 자식 수입니다.
         */
        Index_t GetChildCount() const;

        /**
         * @brief 슬롯 인덱스로 자식 위젯을 조회합니다.
         * @param[in] Index 조회하거나 변경할 0부터 시작하는 인덱스입니다.
         * @return 자식 위젯의 비소유 주소이며 범위를 벗어나면 nullptr입니다.
         */
        Editor::FEditorWidget* GetChild(Index_t Index) const;

        /**
         * @brief 자식 슬롯의 크기 규칙·크기 또는 가중치·여백을 갱신합니다.
         * @param[in] Index 조회하거나 변경할 0부터 시작하는 인덱스입니다.
         * @param[in] SizeRule 자식의 자동·고정·공간 채움 크기 규칙입니다.
         * @param[in] Extent 고정 크기 또는 공간 채움 가중치입니다.
         * @param[in] Padding 자식의 왼쪽·위·오른쪽·아래 내부 여백입니다.
         * @return 슬롯이 존재하고 값이 유한해 적용했으면 true입니다.
         */
        bool SetChildLayout(Index_t Index, Editor::EEditorWidgetSizeRule SizeRule, float Extent, const FVector4& Padding = {});

        /**
         * @brief 자식의 수평·수직·겹침 배치 방식을 지정합니다.
         * @param[in] Layout 자식 위젯을 배치할 방식입니다.
         */
        void SetLayout(Editor::EEditorPanelLayout Layout);

        /**
         * @brief 패널의 자식 배치 방식을 반환합니다.
         * @return 현재 배치 방식입니다.
         */
        Editor::EEditorPanelLayout GetLayout() const;

        /**
         * @brief 자식 간 간격을 유한한 0 이상 값으로 설정합니다.
         * @param[in] Spacing 자식 위젯 사이의 간격입니다.
         */
        void SetSpacing(float Spacing);

        /**
         * @brief 자식 간 배치 간격을 반환합니다.
         * @return 현재 간격입니다.
         */
        float GetSpacing() const;

        /**
         * @brief 표시 중인 자식의 크기 규칙과 여백으로 위치·크기를 계산합니다.
         * @param[in] Size 자식 배치에 사용할 수 있는 패널의 가로·세로 크기입니다.
         * @return 패널 원점 기준 자식 배치 목록입니다.
         */
        Core::TArray<Editor::FEditorPanel::FArrangement> ArrangeChildren(const FVector2& Size) const;

        /**
         * @brief 위젯이 배치에 요청하는 크기를 계산합니다.
         * @return 위젯의 희망 가로·세로 크기입니다.
         */
        FVector2 ComputeDesiredSize() const override;

    private:
        /**
         * @brief 자식 배치를 계산하고 각 콘텐츠를 해당 영역의 자식 창에 그립니다.
         * @param[in] DeltaTime 이전 프레임 이후 경과한 시간이며 단위는 초입니다.
         */
        void DrawContent(float DeltaTime) override;

        /**
         * @brief 현재 등록된 자식 위젯에 숨김 통지를 전달합니다.
         */
        void OnHidden() override;

        /**
         * @brief 중복 부모·탭 연결과 부모 순환 없이 자식을 추가할 수 있는지 검사합니다.
         * @param[in] Widget 연결하거나 등록할 콘텐츠 위젯입니다.
         * @return 부모나 탭이 없고 순환이 생기지 않으면 true입니다.
         */
        bool CanAddChild(const Editor::FEditorWidget& Widget) const;

    private:
        /**
         * @brief 자식 위젯의 연결·소유권과 배치 규칙을 담는 슬롯 목록입니다.
         */
        Core::TArray<FSlot> mChildren{};

        /**
         * @brief 자식 위젯의 수평·수직·겹침 배치 방식입니다.
         */
        Editor::EEditorPanelLayout mLayout{Editor::EEditorPanelLayout::Vertical};

        /**
         * @brief 수평·수직 배치에서 자식 사이에 둘 간격입니다.
         */
        float mSpacing{};
    };
}
