/**
 * @file FHeightmap.cpp
 * @brief 16bit 흑백 PNG 하이트맵 읽기의 구현입니다.
 * @ingroup Asset
 */

#include "Asset/FHeightmap.h"
#include "Core/FilePath.h"

#include <cassert>
#include <fstream>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>

// 다른 라이브러리에 포함된 stb와 심볼이 겹치지 않도록 이 번역 단위 전용으로 구현합니다.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

namespace Asset {

    /**
     * @brief 16bit 흑백 PNG 파일을 읽어 높이 배열을 교체합니다.
     */
    bool Asset::FHeightmap::LoadFromFile(const std::filesystem::path& Path, Core::FString& Error) {
        Error.clear();

        // stb의 파일 함수는 유니코드 경로를 바로 받지 못하므로 파일을 직접 읽어 메모리에서 디코딩합니다.
        std::error_code Code{};
        const auto Size{std::filesystem::file_size(Path, Code)};
        std::ifstream Input{Path, std::ios::binary};

        if (Code || Size == 0 || Size > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()) || !Input) {
            Error = "Cannot open the heightmap file: " + Core::FilePath::ToUtf8(Path);

            return false;
        }

        Core::TArray<Uint8> Bytes(static_cast<std::size_t>(Size));

        if (!Input.read(reinterpret_cast<char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()))) {
            Error = "Cannot read the heightmap file: " + Core::FilePath::ToUtf8(Path);

            return false;
        }

        const int Length{static_cast<int>(Bytes.size())};
        int Width{};
        int Height{};
        int Channels{};

        if (stbi_info_from_memory(Bytes.data(), Length, &Width, &Height, &Channels) == 0) {
            Error = "The heightmap is not a readable PNG: " + Core::FilePath::ToUtf8(Path);

            return false;
        }

        // 8bit 이미지도 stbi_load_16이 16bit로 늘려 읽으므로 원본의 비트 수를 먼저 확인합니다.
        if (stbi_is_16_bit_from_memory(Bytes.data(), Length) == 0) {
            Error = "The heightmap must be a 16-bit PNG: " + Core::FilePath::ToUtf8(Path);

            return false;
        }

        if (Channels != 1) {
            Error = "The heightmap must be a single-channel grayscale PNG: " + Core::FilePath::ToUtf8(Path);

            return false;
        }

        const std::unique_ptr<stbi_us, decltype(&stbi_image_free)> Pixels{stbi_load_16_from_memory(Bytes.data(), Length, &Width, &Height, &Channels, 1), &stbi_image_free};

        if (Pixels == nullptr) {
            Error = "Cannot decode the heightmap: " + Core::FString{stbi_failure_reason()};

            return false;
        }

        const std::size_t Count{static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height)};
        Core::TArray<std::uint16_t> Values(Pixels.get(), Pixels.get() + Count);

        mWidth = static_cast<Uint32>(Width);
        mHeight = static_cast<Uint32>(Height);
        mValues = std::move(Values);

        return true;
    }

    /**
     * @brief 가로 픽셀 수를 반환합니다.
     */
    Uint32 Asset::FHeightmap::GetWidth() const {
        return mWidth;
    }

    /**
     * @brief 세로 픽셀 수를 반환합니다.
     */
    Uint32 Asset::FHeightmap::GetHeight() const {
        return mHeight;
    }

    /**
     * @brief 지정한 픽셀의 높이 값을 반환합니다.
     */
    std::uint16_t Asset::FHeightmap::GetValue(Uint32 X, Uint32 Y) const {
        assert(X < mWidth && Y < mHeight);

        return mValues[static_cast<std::size_t>(Y) * mWidth + X];
    }

}
