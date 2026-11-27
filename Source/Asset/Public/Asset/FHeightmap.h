/**
 * @file FHeightmap.h
 * @brief 16bit 흑백 PNG에서 읽은 CPU 높이 배열입니다.
 * @ingroup Asset
 */

#pragma once

#include "Core/Types.h"

#include <cstdint>
#include <filesystem>

namespace Asset {

    /**
     * @brief 지형의 격자 높이로 사용하는 16bit 값의 2차원 배열입니다.
     * @details 값은 행 우선 순서로 저장하며, 첫 행은 이미지의 위쪽 행입니다.
     * @ingroup Asset
     */
    class FHeightmap {
    public:
        /**
         * @brief 16bit 흑백 PNG 파일을 읽어 높이 배열을 교체합니다.
         * @param Path 읽을 PNG 파일의 경로입니다.
         * @param[out] Error 실패 원인을 받을 오류 문자열입니다.
         * @return 읽기에 성공하면 true입니다. 실패하면 false이며 기존 데이터는 바뀌지 않습니다.
         */
        bool LoadFromFile(const std::filesystem::path& Path, Core::FString& Error);

        /**
         * @brief 가로 픽셀 수를 반환합니다.
         * @return 가로 픽셀 수이며, 읽은 데이터가 없으면 0입니다.
         */
        Uint32 GetWidth() const;

        /**
         * @brief 세로 픽셀 수를 반환합니다.
         * @return 세로 픽셀 수이며, 읽은 데이터가 없으면 0입니다.
         */
        Uint32 GetHeight() const;

        /**
         * @brief 지정한 픽셀의 높이 값을 반환합니다.
         * @param X 왼쪽부터 센 열이며 GetWidth()보다 작아야 합니다.
         * @param Y 위쪽부터 센 행이며 GetHeight()보다 작아야 합니다.
         * @return 0부터 65535 사이의 원본 높이 값입니다.
         */
        std::uint16_t GetValue(Uint32 X, Uint32 Y) const;

    private:
        /**
         * @brief 가로 픽셀 수입니다.
         */
        Uint32 mWidth{};

        /**
         * @brief 세로 픽셀 수입니다.
         */
        Uint32 mHeight{};

        /**
         * @brief 행 우선 순서의 높이 값입니다.
         */
        Core::TArray<std::uint16_t> mValues{};
    };

}
