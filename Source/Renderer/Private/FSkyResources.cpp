/**
 * @file FSkyResources.cpp
 * @brief 등장방형 파노라마를 전체 화면 삼각형으로 그리는 기본 하늘 패스의 자원입니다.
 * @ingroup Renderer
 */

#include "Core/Types.h"
#include "FSkyResources.h"
#include "FPassTarget.h"
#include "Core/Base/ErrorHandler.h"
#include "Core/Math/FMatrix.h"
#include "Engine/FSceneView.h"

#include "Asset/FAssetManager.h"
#include "Asset/UTexture2D.h"

namespace Renderer {

    /**
     * @brief 기본 설정으로 FSkyResources 객체를 초기화합니다.
     */
    Renderer::FSkyResources::FSkyResources() = default;

    /**
     * @brief 객체 수명을 끝내고 소유한 자원을 정리합니다.
     */
    Renderer::FSkyResources::~FSkyResources() = default;

    /**
     * @brief 상수 버퍼·샘플러·깊이·래스터라이저 상태를 생성하고 파노라마를 불러옵니다.
     */
    bool Renderer::FSkyResources::Initialize(ID3D11Device* Device, const std::filesystem::path& PanoramaPath, const std::filesystem::path& PreviewPath) {
        D3D11_SAMPLER_DESC Sampler{};
        Sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        Sampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        Sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        Sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        Sampler.MaxLOD = D3D11_FLOAT32_MAX;

        D3D11_DEPTH_STENCIL_DESC Depth{};
        Depth.DepthEnable = true;
        Depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        Depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        D3D11_RASTERIZER_DESC Rasterizer{};
        Rasterizer.FillMode = D3D11_FILL_SOLID;
        Rasterizer.CullMode = D3D11_CULL_NONE;

        Core::FString Error{};

        if (!mConstants.Create(Device, Error) || FAILED(Device->CreateSamplerState(&Sampler, mSampler.GetAddressOf())) || FAILED(Device->CreateDepthStencilState(&Depth, mDepthState.GetAddressOf())) || FAILED(Device->CreateRasterizerState(&Rasterizer, mRasterizer.GetAddressOf()))) {
            return false;
        }

        if (!LoadPanorama(Device, PanoramaPath, mPanorama)) {
            Core::ErrorHandler::Report("Sky", "Cannot load the sky panorama: " + PanoramaPath.string(), Core::ErrorHandler::EErrorLevel::Warning);
        }

        if (!LoadPanorama(Device, PreviewPath, mPreviewPanorama)) {
            Core::ErrorHandler::Report("Sky", "Cannot load the preview panorama: " + PreviewPath.string(), Core::ErrorHandler::EErrorLevel::Warning);
        }

        return true;
    }

    /**
     * @brief 정점·픽셀 바이트 코드로 하늘 셰이더를 생성하고 둘 다 성공한 경우에만 교체합니다.
     */
    bool Renderer::FSkyResources::CreateShaders(ID3D11Device* Device, std::span<const Uint8> VertexCode, std::span<const Uint8> PixelCode) {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> Vertex{};
        Microsoft::WRL::ComPtr<ID3D11PixelShader> Pixel{};

        if (FAILED(Device->CreateVertexShader(VertexCode.data(), VertexCode.size(), nullptr, Vertex.GetAddressOf())) || FAILED(Device->CreatePixelShader(PixelCode.data(), PixelCode.size(), nullptr, Pixel.GetAddressOf()))) {
            return false;
        }

        mVertexShader = std::move(Vertex);
        mPixelShader = std::move(Pixel);

        return true;
    }

    /**
     * @brief 불투명 패스의 깊이가 비어 있는 픽셀에만 하늘을 그립니다.
     */
    Uint32 Renderer::FSkyResources::Render(ID3D11DeviceContext* Context, const Renderer::FPassTarget& Target, const Engine::FSceneView& View, bool Preview) {
        ID3D11ShaderResourceView* Panorama{Preview && mPreviewPanorama != nullptr ? mPreviewPanorama.Get() : mPanorama.Get()};

        if (Panorama == nullptr || mVertexShader == nullptr || mPixelShader == nullptr) {
            return 0;
        }

        // 파노라마는 방향만 필요하므로 이동을 빼서 카메라가 멀리 있어도 정밀도를 유지합니다.
        FMatrix ViewRotation{View.mView};

        ViewRotation(3, 0) = 0.0f;
        ViewRotation(3, 1) = 0.0f;
        ViewRotation(3, 2) = 0.0f;

        FMatrix InverseViewRotationProjection{};

        if (!(ViewRotation * View.mProjection).TryInverse(InverseViewRotationProjection)) {
            return 0;
        }

        mConstants.Update(Context, InverseViewRotationProjection);
        Context->OMSetRenderTargets(1, &Target.mColor, Target.mDepth);
        Context->OMSetDepthStencilState(mDepthState.Get(), 0);
        Context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);
        Context->RSSetViewports(1, &Target.mViewport);
        Context->RSSetState(mRasterizer.Get());
        Context->IASetInputLayout(nullptr);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->VSSetShader(mVertexShader.Get(), nullptr, 0);
        Context->PSSetShader(mPixelShader.Get(), nullptr, 0);

        ID3D11SamplerState* Sampler{mSampler.Get()};

        mConstants.Bind(Context, RenderCore::EShaderStage::Pixel, 0);
        Context->PSSetSamplers(0, 1, &Sampler);
        Context->PSSetShaderResources(0, 1, &Panorama);
        Context->Draw(3, 0);

        ID3D11ShaderResourceView* Empty{};

        Context->PSSetShaderResources(0, 1, &Empty);

        return 1;
    }

    /**
     * @brief 하늘 텍스처를 표시 공간의 값으로 샘플링하도록 GPU 자원을 생성합니다.
     */
    bool Renderer::FSkyResources::LoadPanorama(ID3D11Device* Device, const std::filesystem::path& Path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& Panorama) {
        Asset::FAssetManager Assets{Path.parent_path()};
        Core::FString Error{};
        auto* Texture{Assets.LoadTexture(Path.filename(), Error)};
        if (Texture == nullptr || !Texture->SetSRGB(false) || !Texture->CreateResources(Device)) {
            return false;
        }

        Panorama = Texture->GetShaderResourceView();

        return Panorama != nullptr;
    }

}
