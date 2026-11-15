/**
 * @file EditorStyle.h
 * @brief 에디터에서 공유하는 벡터 아이콘과 툴바 표시 함수를 선언합니다.
 * @ingroup Editor
 */

#pragma once

#include <imgui.h>
#include "Editor/EEditorIcon.h"

#include <filesystem>
#include <functional>

namespace Editor {

    /**
     * @brief 에디터 UI의 아이콘, 버튼과 툴바 스타일을 제공합니다.
     * @ingroup Editor
     * @note 함수는 현재 ImGui 컨텍스트와 창을 사용하며 뷰포트 명령이나 편집 상태를 소유하지 않습니다.
     */
    namespace EditorStyle {
        /**
         * @brief 명령 정의와 UI가 공유하는 아이콘 식별자의 스타일 네임스페이스 별칭입니다.
         */
        using EIcon = Editor::EEditorIcon;

        /**
         * @brief 현재 ImGui 스타일 스택에 뷰포트 툴바의 간격, 모서리와 버튼 색상을 적용합니다.
         * @note 같은 컨텍스트에서 PopViewportToolbar()와 짝지어 호출해야 합니다.
         */
        void PushViewportToolbar();

        /**
         * @brief PushViewportToolbar()로 적용한 스타일을 복원합니다.
         * @pre 대응하는 PushViewportToolbar() 호출이 있어야 합니다.
         */
        void PopViewportToolbar();

        /**
         * @brief 현재 창의 드로 목록에 지정한 벡터 아이콘을 그립니다.
         * @param[in] Icon 표시할 공통 아이콘입니다.
         * @param[in] Origin 아이콘 정사각형의 화면 기준 왼쪽 위 픽셀 좌표입니다.
         * @param[in] Size 아이콘 정사각형의 한 변 길이이며 양수여야 합니다.
         * @param[in] Color 알파를 포함하는 ImGui 형식의 패킹된 표시 색상입니다.
         * @pre 현재 ImGui 프레임과 창이 유효해야 합니다.
         */
        void DrawIcon(EIcon Icon, const ImVec2& Origin, float Size, ImU32 Color);

        /**
         * @brief 지정한 드로 목록에 벡터 아이콘을 그립니다.
         * @param[in,out] DrawList 아이콘을 추가할 드로 목록입니다.
         * @param[in] Icon 그릴 아이콘입니다.
         * @param[in] Origin 아이콘 왼쪽 위의 화면 좌표입니다.
         * @param[in] Size 아이콘 한 변의 픽셀 크기입니다.
         * @param[in] Color 아이콘 색상입니다.
         */
        void DrawIcon(ImDrawList* DrawList, EIcon Icon, const ImVec2& Origin, float Size, ImU32 Color);

        /**
         * @brief 탭 아이콘이 들어갈 자리를 비우기 위해 창 제목 앞에 붙이는 공백입니다.
         */
        inline constexpr const char* TabIconPadding{"      "};

        /**
         * @brief 방금 Begin()한 창이 도킹되어 있으면 그 탭의 왼쪽에 아이콘을 그립니다.
         * @param[in] Icon 탭에 그릴 아이콘입니다.
         * @param[in] Color 아이콘 색상이며 0이면 텍스트 색상을 사용합니다.
         * @note 창 제목은 TabIconPadding으로 시작해야 아이콘과 글자가 겹치지 않습니다.
         */
        void TabIcon(EIcon Icon, ImU32 Color = 0);

        /**
         * @brief 선택 및 비활성 상태를 반영하는 아이콘 버튼과 툴팁을 표시합니다.
         * @param[in] Label 현재 ID 범위에서 버튼을 식별하며 기본 툴팁에도 사용할 문자열입니다.
         * @param[in] Icon 버튼에 표시할 아이콘입니다.
         * @param[in] Selected 선택 색상을 적용할지 지정하며 외부 상태를 변경하지 않습니다.
         * @param[in] Enabled false이면 버튼 클릭을 차단하며 툴팁은 계속 표시할 수 있습니다.
         * @param[in] Dropdown true이면 드롭다운 화살표와 그 공간을 추가합니다.
         * @param[in] Tooltip 대체 툴팁이며 nullptr이면 Label을 사용합니다.
         * @return 활성 버튼을 눌렀으면 true이며 비활성 상태에서는 false입니다.
         * @note 팝업 열기와 명령 실행은 호출자가 처리합니다. 문자열은 호출 중 유효해야 합니다.
         */
        bool IconButton(const char* Label, EIcon Icon, bool Selected = false, bool Enabled = true, bool Dropdown = false, const char* Tooltip = nullptr);

        /**
         * @brief 메뉴의 아이콘 열을 확보하고 선택 및 비활성 상태를 반영한 항목을 표시합니다.
         * @param[in] Label 표시할 메뉴 항목 이름이며 호출 중 유효해야 합니다.
         * @param[in] Icon 항목 왼쪽에 표시할 아이콘입니다.
         * @param[in] Selected 현재 선택을 나타내는 체크 표시 여부입니다.
         * @param[in] Enabled false이면 항목 선택을 차단합니다.
         * @return 활성 항목을 선택했으면 true이며 비활성 상태에서는 false입니다.
         * @pre 현재 ImGui 메뉴 또는 팝업 안에서 호출해야 합니다.
         */
        /**
         * @brief 선택적인 아이콘과 드롭다운 표시를 붙인 텍스트 버튼을 표시합니다.
         * @param[in] Label 버튼에 표시할 문자열이며 ## 뒤는 식별자로만 사용합니다.
         * @param[in] Icon 텍스트 앞에 그릴 아이콘이며 없으면 nullptr입니다.
         * @param[in] Dropdown true이면 오른쪽에 드롭다운 삼각형을 그립니다.
         * @param[in] Enabled false이면 비활성 상태로 표시합니다.
         * @param[in] Tooltip 마우스를 올렸을 때 보여 줄 설명이며 없으면 nullptr입니다.
         * @return 버튼을 눌렀으면 true입니다.
         */
        bool LabelButton(const char* Label, const EIcon* Icon = nullptr, bool Dropdown = false, bool Enabled = true, const char* Tooltip = nullptr);

        /**
         * @brief 에셋 에디터 툴바의 찾아보기 버튼이 콘텐츠 브라우저에서 에셋을 선택할 때 호출할 처리기를 지정합니다.
         * @param[in] Handler 에셋 경로를 받아 콘텐츠 브라우저를 이동시키는 함수이며 비우면 버튼을 비활성화합니다.
         */
        void SetAssetBrowseHandler(std::function<void(const std::filesystem::path&)> Handler);

        /**
         * @brief 에셋 에디터 탭 위쪽에 저장·콘텐츠 브라우저에서 찾기 버튼 줄을 표시합니다.
         * @param[in] AssetPath 찾아보기 버튼이 선택할 에셋 경로입니다.
         * @param[in] CanSave 저장 버튼을 활성화할지 지정합니다.
         * @param[in] ShowSave false이면 저장 버튼을 표시하지 않습니다.
         * @return 저장 버튼을 눌렀으면 true입니다.
         */
        bool AssetToolbar(const std::filesystem::path& AssetPath, bool CanSave, bool ShowSave = true);

        bool IconMenuItem(const char* Label, EIcon Icon, bool Selected, bool Enabled = true);
    }

}
