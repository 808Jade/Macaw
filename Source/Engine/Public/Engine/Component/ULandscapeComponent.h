/**
 * @file ULandscapeComponent.h
 * @brief 하이트맵으로 만든 지형 메시를 그리는 컴포넌트입니다.
 * @ingroup Engine
 */

#pragma once

#include "Core/ReflectionMacros.h"
#include "Core/Literals/QuantityTypes.h"
#include "Engine/Component/UMeshComponent.h"
#include "Engine/EPrimitiveType.h"

#include <memory>

#include "ULandscapeComponent.generated.h"

namespace Engine {

    /**
     * @brief 16bit 흑백 PNG 하이트맵에서 지형 메시 하나를 만들어 소유하는 컴포넌트입니다.
     * @details 하이트맵 경로와 크기 설정만 저장하며, 메시는 월드에 에셋 관리자가 연결되면 다시 만듭니다.
     *          충돌 형상은 제공하지 않습니다.
     * @ingroup Engine
     */
    UCLASS()
    class ULandscapeComponent final : public Engine::UMeshComponent {
        GENERATED_BODY()

    public:
        /**
         * @brief 간격 1m, 최대 높이 20m이고 하이트맵이 없는 컴포넌트를 구성합니다.
         */
        ULandscapeComponent();

        /**
         * @brief 소유한 지형 메시를 해제합니다.
         */
        ~ULandscapeComponent() override;

    public:
        /**
         * @brief 기본 프리미티브 종류를 반환합니다.
         * @return 항상 Cube이며, 지형 메시가 없으면 렌더 상태를 만들지 않으므로 그려지지 않습니다.
         */
        Engine::EPrimitiveType GetPrimitiveType() const override;

        /**
         * @brief 하이트맵의 콘텐츠 경로를 반환합니다.
         * @return UTF-8 경로이며, 지정하지 않았으면 비어 있습니다.
         */
        const Core::FString& GetHeightmapPath() const;

        /**
         * @brief 하이트맵을 바꾸고 지형 메시를 다시 만듭니다.
         * @param Path 콘텐츠 폴더 기준의 PNG 경로입니다. 비우면 지형을 제거합니다.
         * @return 반영하면 true입니다. 월드에서 파일을 읽을 수 없으면 false이며 기존 값을 유지합니다.
         */
        bool SetHeightmapPath(const Core::FString& Path);

        /**
         * @brief 이웃한 정점 사이의 수평 간격을 반환합니다.
         * @return 정점 간격입니다.
         */
        Core::FLength GetCellSize() const;

        /**
         * @brief 정점 간격을 바꾸고 지형 메시를 다시 만듭니다.
         * @param CellSize 0보다 큰 정점 간격입니다.
         * @return 값이 유효해 반영하면 true입니다.
         */
        bool SetCellSize(Core::FLength CellSize);

        /**
         * @brief 하이트맵 값 65535에 대응하는 높이를 반환합니다.
         * @return 지형의 최대 높이입니다.
         */
        Core::FLength GetMaxHeight() const;

        /**
         * @brief 최대 높이를 바꾸고 지형 메시를 다시 만듭니다.
         * @param MaxHeight 0 이상의 높이입니다.
         * @return 값이 유효해 반영하면 true입니다.
         */
        bool SetMaxHeight(Core::FLength MaxHeight);

        /**
         * @brief 만들어진 지형 메시를 반환합니다.
         * @return 컴포넌트가 소유한 메시의 비소유 주소이며, 지형이 없으면 nullptr입니다.
         */
        Asset::UStaticMesh* GetStaticMesh() const override;

        /**
         * @brief 지형의 CPU 메시 데이터를 반환합니다.
         * @return 지형이 없으면 빈 데이터의 참조입니다.
         */
        const Asset::FMeshData& GetMeshData() const override;

        /**
         * @brief 현재 설정으로 하이트맵을 읽어 지형 메시를 다시 만듭니다.
         * @return 하이트맵 경로가 비어 있거나 메시를 만들면 true입니다.
         */
        bool ResolveMesh() override;

        /**
         * @brief 그릴 지형 메시가 있는지 반환합니다.
         * @return 지형 메시가 있으면 true입니다.
         */
        bool ShouldCreateRenderState() const override;

    private:
        /**
         * @brief 지정한 하이트맵과 현재 크기 설정으로 지형 메시를 만듭니다.
         * @param Path 콘텐츠 폴더 기준의 PNG 경로입니다.
         * @return 성공하면 true입니다. 실패하면 false이며 기존 메시를 바꾸지 않습니다.
         */
        bool BuildMesh(const Core::FString& Path);

    private:
        /**
         * @brief 콘텐츠 폴더 기준의 하이트맵 경로입니다.
         */
        UPROPERTY(Edit, Serialize, Category = "Landscape", Getter = GetHeightmapPath, Setter = SetHeightmapPath)
        Core::FString mHeightmapPath{};

        /**
         * @brief 정점 사이의 수평 간격입니다.
         */
        UPROPERTY(Edit, Serialize, Category = "Landscape", Getter = GetCellSize, Setter = SetCellSize)
        Core::FLength mCellSize{};

        /**
         * @brief 하이트맵 최댓값에 대응하는 높이입니다.
         */
        UPROPERTY(Edit, Serialize, Category = "Landscape", Getter = GetMaxHeight, Setter = SetMaxHeight)
        Core::FLength mMaxHeight{};

        /**
         * @brief 지형 메시입니다. 렌더 프록시가 주소를 참조하므로 한 번 만들면 객체를 유지하고 내용만 교체합니다.
         */
        std::unique_ptr<Asset::UStaticMesh> mLandscapeMesh{};

        /**
         * @brief 현재 설정에 해당하는 지형 메시가 있는지 나타냅니다.
         */
        bool mHasMesh{};
    };

}
