/**
 * @file EEditorViewportLayoutPreset.h
 * @brief 뷰포트 분할 트리의 기본 배치 프리셋입니다.
 * @ingroup Editor
 */

#pragma once

namespace Editor {
    /**
     * @brief 뷰포트 분할 트리의 기본 배치 프리셋입니다.
     */
    enum class EEditorViewportLayoutPreset {
        /**
         * @brief 뷰포트 하나를 전체 영역에 표시합니다.
         */
        Single,

        /**
         * @brief 두 뷰포트를 좌우로 배치합니다.
         */
        TwoColumns,

        /**
         * @brief 두 뷰포트를 위아래로 배치합니다.
         */
        TwoRows,

        /**
         * @brief 왼쪽 큰 뷰포트와 오른쪽 두 뷰포트를 배치합니다.
         */
        ThreeLeft,

        /**
         * @brief 오른쪽 큰 뷰포트와 왼쪽 두 뷰포트를 배치합니다.
         */
        ThreeRight,

        /**
         * @brief 위쪽 큰 뷰포트와 아래쪽 두 뷰포트를 배치합니다.
         */
        ThreeTop,

        /**
         * @brief 아래쪽 큰 뷰포트와 위쪽 두 뷰포트를 배치합니다.
         */
        ThreeBottom,

        /**
         * @brief 뷰포트 네 개를 2×2 격자로 배치합니다.
         */
        FourGrid,

        /**
         * @brief 왼쪽 큰 뷰포트와 오른쪽 세 뷰포트를 배치합니다.
         */
        FourLeft,

        /**
         * @brief 오른쪽 큰 뷰포트와 왼쪽 세 뷰포트를 배치합니다.
         */
        FourRight,

        /**
         * @brief 위쪽 큰 뷰포트와 아래쪽 세 뷰포트를 배치합니다.
         */
        FourTop,

        /**
         * @brief 아래쪽 큰 뷰포트와 위쪽 세 뷰포트를 배치합니다.
         */
        FourBottom,

        /**
         * @brief 프리셋 개수이며 실제 배치로 사용하지 않습니다.
         */
        Count
    };
}
