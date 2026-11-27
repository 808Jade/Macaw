/**
 * @file ULandscapeComponent.cpp
 * @brief 하이트맵 지형 컴포넌트의 구현입니다.
 * @ingroup Engine
 */

#include "Core/Types.h"
#include "Engine/Component/ULandscapeComponent.h"
#include "Asset/FAssetManager.h"
#include "Asset/FHeightmap.h"
#include "Asset/FLandscapeMeshBuilder.h"
#include "Asset/UStaticMesh.h"
#include "Core/Base/ErrorHandler.h"
#include "Core/FilePath.h"
#include "Core/Literals/UnitLiterals.h"
#include "CoreUObject/Reflection/NewObject.h"
#include "Engine/UWorld.h"

#include <utility>

namespace Engine {

    using namespace Core::Literals;

    /**
     * @brief 간격 1m, 최대 높이 20m이고 하이트맵이 없는 컴포넌트를 구성합니다.
     */
    Engine::ULandscapeComponent::ULandscapeComponent()
        : mCellSize{1_m},
          mMaxHeight{20_m} {
    }

    /**
     * @brief 소유한 지형 메시를 해제합니다.
     */
    Engine::ULandscapeComponent::~ULandscapeComponent() = default;

    /**
     * @brief 기본 프리미티브 종류를 반환합니다.
     */
    Engine::EPrimitiveType Engine::ULandscapeComponent::GetPrimitiveType() const {
        return Engine::EPrimitiveType::Cube;
    }

    /**
     * @brief 하이트맵의 콘텐츠 경로를 반환합니다.
     */
    const Core::FString& Engine::ULandscapeComponent::GetHeightmapPath() const {
        return mHeightmapPath;
    }

    /**
     * @brief 하이트맵을 바꾸고 지형 메시를 다시 만듭니다.
     */
    bool Engine::ULandscapeComponent::SetHeightmapPath(const Core::FString& Path) {
        if (Path.find('\0') != Core::FString::npos) {
            return false;
        }

        if (mHeightmapPath == Path && (Path.empty() || mHasMesh)) {
            return true;
        }

        // 역직렬화처럼 에셋 관리자가 아직 없으면 경로만 저장하고, 등록할 때 ResolveMesh가 메시를 만듭니다.
        const bool CanLoad{GetWorld() != nullptr && GetWorld()->GetAssets() != nullptr};

        if (!Path.empty() && CanLoad) {
            if (!BuildMesh(Path)) {
                return false;
            }

            mHasMesh = true;
        } else {
            mHasMesh = false;
        }

        mHeightmapPath = Path;
        MarkRenderStateDirty();

        return true;
    }

    /**
     * @brief 이웃한 정점 사이의 수평 간격을 반환합니다.
     */
    Core::FLength Engine::ULandscapeComponent::GetCellSize() const {
        return mCellSize;
    }

    /**
     * @brief 정점 간격을 바꾸고 지형 메시를 다시 만듭니다.
     */
    bool Engine::ULandscapeComponent::SetCellSize(Core::FLength CellSize) {
        if (!CellSize.IsFinite() || CellSize <= 0_m) {
            return false;
        }

        if (mCellSize != CellSize) {
            mCellSize = CellSize;
            ResolveMesh();
        }

        return true;
    }

    /**
     * @brief 하이트맵 값 65535에 대응하는 높이를 반환합니다.
     */
    Core::FLength Engine::ULandscapeComponent::GetMaxHeight() const {
        return mMaxHeight;
    }

    /**
     * @brief 최대 높이를 바꾸고 지형 메시를 다시 만듭니다.
     */
    bool Engine::ULandscapeComponent::SetMaxHeight(Core::FLength MaxHeight) {
        if (!MaxHeight.IsFinite() || MaxHeight < 0_m) {
            return false;
        }

        if (mMaxHeight != MaxHeight) {
            mMaxHeight = MaxHeight;
            ResolveMesh();
        }

        return true;
    }

    /**
     * @brief 만들어진 지형 메시를 반환합니다.
     */
    Asset::UStaticMesh* Engine::ULandscapeComponent::GetStaticMesh() const {
        return mHasMesh ? mLandscapeMesh.get() : nullptr;
    }

    /**
     * @brief 지형의 CPU 메시 데이터를 반환합니다.
     */
    const Asset::FMeshData& Engine::ULandscapeComponent::GetMeshData() const {
        if (mHasMesh) {
            return mLandscapeMesh->GetMeshData();
        }

        static const Asset::FMeshData EmptyMesh{};

        return EmptyMesh;
    }

    /**
     * @brief 현재 설정으로 하이트맵을 읽어 지형 메시를 다시 만듭니다.
     */
    bool Engine::ULandscapeComponent::ResolveMesh() {
        mHasMesh = !mHeightmapPath.empty() && BuildMesh(mHeightmapPath);
        MarkRenderStateDirty();

        return mHeightmapPath.empty() || mHasMesh;
    }

    /**
     * @brief 그릴 지형 메시가 있는지 반환합니다.
     */
    bool Engine::ULandscapeComponent::ShouldCreateRenderState() const {
        // 메시 없는 프록시는 렌더러가 기본 프리미티브로 대신 그리므로 만들지 않습니다.
        return mHasMesh;
    }

    /**
     * @brief 지정한 하이트맵과 현재 크기 설정으로 지형 메시를 만듭니다.
     */
    bool Engine::ULandscapeComponent::BuildMesh(const Core::FString& Path) {
        Asset::FAssetManager* Assets{GetWorld() != nullptr ? GetWorld()->GetAssets() : nullptr};

        if (Assets == nullptr) {
            return false;
        }

        Core::FString Error{};
        const auto ResolvedPath{Assets->ResolvePath(Core::FilePath::FromUtf8(Path), Error)};
        Asset::FHeightmap Heightmap{};
        Asset::FMeshData MeshData{};

        if (ResolvedPath.empty() || !Heightmap.LoadFromFile(ResolvedPath, Error) || !Asset::FLandscapeMeshBuilder::Build(Heightmap, mCellSize, mMaxHeight, MeshData, Error)) {
            Core::ErrorHandler::Report("Landscape", "Cannot build the landscape from '" + Path + "': " + Error, Core::ErrorHandler::EErrorLevel::Warning);

            return false;
        }

        auto NewMesh{mLandscapeMesh == nullptr ? CoreUObject::NewObject<Asset::UStaticMesh>() : nullptr};
        Asset::UStaticMesh* Mesh{NewMesh != nullptr ? NewMesh.get() : mLandscapeMesh.get()};

        if (!Mesh->SetMeshData(std::move(MeshData))) {
            Core::ErrorHandler::Report("Landscape", "The landscape mesh from '" + Path + "' is invalid", Core::ErrorHandler::EErrorLevel::Warning);

            return false;
        }

        if (NewMesh != nullptr) {
            NewMesh->SetName("LandscapeMesh");
            NewMesh->SetOuter(this);
            mLandscapeMesh = std::move(NewMesh);
        }

        return true;
    }

}
