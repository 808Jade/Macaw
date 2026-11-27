/**
 * @file FTerrainMeshBuilder.h
 * @brief 하이트맵에서 지형의 CPU 메시 데이터를 만드는 생성기입니다.
 * @ingroup Asset
 */

#pragma once

#include "Core/Literals/QuantityTypes.h"
#include "Core/Types.h"

namespace Asset {

    struct FMeshData;
    class FHeightmap;

    /**
     * @brief 하이트맵의 픽셀 하나를 정점 하나로 하는 격자 메시를 만듭니다.
     * @ingroup Asset
     */
    class FTerrainMeshBuilder {
    public:
        /**
         * @brief 하이트맵으로 위치·법선·탄젠트·UV·인덱스를 채웁니다.
         * @details 메시는 로컬 원점을 XY 중심으로 하며 높이는 +Z 방향입니다. 이미지의 오른쪽은 +X, 아래쪽은 +Y입니다.
         *          법선은 이웃 픽셀의 높이 차이로 계산하고, UV는 지형 전체를 0부터 1까지로 덮습니다.
         *          서브메시와 재질 슬롯은 비워 두며 UStaticMesh::SetMeshData가 기본값을 채웁니다.
         * @param Heightmap 가로와 세로가 각각 2픽셀 이상인 높이 배열입니다.
         * @param CellSize 이웃한 정점 사이의 수평 간격이며 0보다 커야 합니다.
         * @param MaxHeight 높이 값 65535에 대응하는 높이이며 0 이상이어야 합니다.
         * @param[out] MeshData 생성한 메시를 받습니다. 실패하면 바뀌지 않습니다.
         * @param[out] Error 실패 원인을 받을 오류 문자열입니다.
         * @return 생성에 성공하면 true입니다.
         */
        static bool Build(const Asset::FHeightmap& Heightmap, Core::FLength CellSize, Core::FLength MaxHeight, Asset::FMeshData& MeshData, Core::FString& Error);
    };

}
