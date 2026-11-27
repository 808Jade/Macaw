/**
 * @file FTerrainMeshBuilder.cpp
 * @brief 하이트맵 지형 메시 생성의 구현입니다.
 * @ingroup Asset
 */

#include "Asset/FTerrainMeshBuilder.h"
#include "Asset/FHeightmap.h"
#include "Asset/FMeshData.h"
#include "Core/Math/FVector.h"

#include <cmath>
#include <limits>
#include <utility>

namespace Asset {

    /**
     * @brief 하이트맵으로 위치·법선·탄젠트·UV·인덱스를 채웁니다.
     */
    bool Asset::FTerrainMeshBuilder::Build(const Asset::FHeightmap& Heightmap, Core::FLength CellSize, Core::FLength MaxHeight, Asset::FMeshData& MeshData, Core::FString& Error) {
        Error.clear();

        const Uint32 Width{Heightmap.GetWidth()};
        const Uint32 Height{Heightmap.GetHeight()};

        if (Width < 2 || Height < 2) {
            Error = "The heightmap must be at least 2x2 pixels";

            return false;
        }

        const float Spacing{CellSize.GetBase()};
        const float Amplitude{MaxHeight.GetBase()};

        if (!std::isfinite(Spacing) || Spacing <= 0.0f || !std::isfinite(Amplitude) || Amplitude < 0.0f) {
            Error = "The terrain cell size must be positive and the maximum height must not be negative";

            return false;
        }

        // 메시 검증과 GPU 버퍼가 허용하는 바이트 크기를 넘기 전에 거부합니다.
        constexpr Uint64 MaximumBytes{std::numeric_limits<Uint32>::max()};
        const Uint64 VertexCount{static_cast<Uint64>(Width) * Height};
        const Uint64 IndexCount{static_cast<Uint64>(Width - 1) * (Height - 1) * 6};

        if (VertexCount > MaximumBytes / (sizeof(float) * 7) || IndexCount > MaximumBytes / sizeof(Uint32)) {
            Error = "The heightmap is too large for a single mesh";

            return false;
        }

        Asset::FMeshData Result{};
        Core::TArray<FVector>& Positions{Result.GetPositions()};
        Core::TArray<FVector>& Normals{Result.GetNormals()};
        Core::TArray<FVector4>& Tangents{Result.GetTangents()};
        Core::TArray<Uint32>& Indices{Result.GetIndices()};

        Result.GetUVChannels().resize(1);

        Core::TArray<FVector2>& UVs{Result.GetUVChannels().front()};

        Positions.reserve(static_cast<std::size_t>(VertexCount));
        Normals.reserve(static_cast<std::size_t>(VertexCount));
        Tangents.reserve(static_cast<std::size_t>(VertexCount));
        UVs.reserve(static_cast<std::size_t>(VertexCount));
        Indices.reserve(static_cast<std::size_t>(IndexCount));

        const float HeightScale{Amplitude / static_cast<float>(std::numeric_limits<std::uint16_t>::max())};
        const float OriginX{-0.5f * Spacing * static_cast<float>(Width - 1)};
        const float OriginY{-0.5f * Spacing * static_cast<float>(Height - 1)};
        const auto GetElevation{[&Heightmap, HeightScale](Uint32 X, Uint32 Y) {
            return static_cast<float>(Heightmap.GetValue(X, Y)) * HeightScale;
        }};

        for (Uint32 Y{}; Y < Height; ++Y) {
            // 가장자리에는 바깥 이웃이 없으므로 자기 자신과의 한쪽 차분을 사용합니다.
            const Uint32 Previous{Y > 0 ? Y - 1 : Y};
            const Uint32 Next{Y + 1 < Height ? Y + 1 : Y};

            for (Uint32 X{}; X < Width; ++X) {
                const Uint32 Left{X > 0 ? X - 1 : X};
                const Uint32 Right{X + 1 < Width ? X + 1 : X};
                const float SlopeX{(GetElevation(Right, Y) - GetElevation(Left, Y)) / (Spacing * static_cast<float>(Right - Left))};
                const float SlopeY{(GetElevation(X, Next) - GetElevation(X, Previous)) / (Spacing * static_cast<float>(Next - Previous))};

                Positions.emplace_back(OriginX + Spacing * static_cast<float>(X), OriginY + Spacing * static_cast<float>(Y), GetElevation(X, Y));
                Normals.push_back(FVector{-SlopeX, -SlopeY, 1.0f}.Normalized());
                Tangents.emplace_back(FVector{1.0f, 0.0f, SlopeX}.Normalized(), 1.0f);
                UVs.emplace_back(static_cast<float>(X) / static_cast<float>(Width - 1), static_cast<float>(Y) / static_cast<float>(Height - 1));
            }
        }

        // 삼각형의 정점 순서는 기본 Plane 프리미티브와 같아야 윗면이 앞면이 됩니다.
        for (Uint32 Y{}; Y + 1 < Height; ++Y) {
            for (Uint32 X{}; X + 1 < Width; ++X) {
                const Uint32 TopLeft{Y * Width + X};
                const Uint32 TopRight{TopLeft + 1};
                const Uint32 BottomLeft{TopLeft + Width};
                const Uint32 BottomRight{BottomLeft + 1};

                Indices.insert(Indices.end(), {TopLeft, TopRight, BottomRight, TopLeft, BottomRight, BottomLeft});
            }
        }

        Result.ComputeBounds();
        MeshData = std::move(Result);

        return true;
    }

}
